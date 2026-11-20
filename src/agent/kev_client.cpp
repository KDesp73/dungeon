#include "agent/kev_client.hpp"

#include <algorithm>
#include <cerrno>
#include <cctype>
#include <cstddef>
#include <cstdlib>
#include <cstring>
#include <string>

#include <arpa/inet.h>
#include <netdb.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>

namespace dungeon::agent {
namespace {

constexpr std::size_t kMaxResponseBytes = 4 * 1024 * 1024;

bool parse_url(const std::string& url, HttpKevClient::Reply& error, std::string& host,
               unsigned short& port, std::string& path) {
    std::size_t position = 0;
    if (url.rfind("http://", 0) == 0) {
        position = 7;
    } else if (url.rfind("https://", 0) == 0) {
        error.error = "https is not supported by the native Kev client";
        return false;
    }

    const std::size_t path_start = url.find('/', position);
    const std::string authority  = url.substr(
        position, path_start == std::string::npos ? std::string::npos : path_start - position);
    if (authority.empty()) {
        error.error = "Kev URL has no host";
        return false;
    }

    path = path_start == std::string::npos ? "/v1/systemone" : url.substr(path_start);
    if (path.empty()) {
        path = "/v1/systemone";
    }

    const std::size_t colon = authority.rfind(':');
    if (colon == std::string::npos) {
        host = authority;
        port = 80;
    } else {
        host = authority.substr(0, colon);
        const std::string port_text = authority.substr(colon + 1);
        if (port_text.empty() ||
            !std::all_of(port_text.begin(), port_text.end(),
                         [](char character) { return character >= '0' && character <= '9'; })) {
            error.error = "Kev URL has an invalid port";
            return false;
        }
        const long parsed = std::strtol(port_text.c_str(), nullptr, 10);
        if (parsed <= 0 || parsed > 65535) {
            error.error = "Kev URL has an out-of-range port";
            return false;
        }
        port = static_cast<unsigned short>(parsed);
    }
    return true;
}

bool send_all(int socket, const char* data, std::size_t size) {
    std::size_t sent = 0;
    while (sent < size) {
        const ssize_t count = ::send(socket, data + sent, size - sent, 0);
        if (count <= 0) {
            if (count < 0 && (errno == EINTR)) {
                continue;
            }
            return false;
        }
        sent += static_cast<std::size_t>(count);
    }
    return true;
}

std::string decode_chunked(const std::string& body) {
    std::string decoded;
    std::size_t position = 0;
    while (position < body.size()) {
        const std::size_t line_end = body.find("\r\n", position);
        if (line_end == std::string::npos) {
            break;
        }
        const std::string size_text = body.substr(position, line_end - position);
        const std::size_t semicolon = size_text.find(';');
        const std::string hex = size_text.substr(0, semicolon);
        if (hex.empty()) {
            break;
        }

        char* end   = nullptr;
        const long length = std::strtol(hex.c_str(), &end, 16);
        if (end == hex.c_str() || length <= 0) {
            break;
        }

        position = line_end + 2;
        if (position + static_cast<std::size_t>(length) > body.size()) {
            break;
        }
        decoded.append(body, position, static_cast<std::size_t>(length));
        position += static_cast<std::size_t>(length) + 2;
    }
    return decoded;
}

} // namespace

HttpKevClient::HttpKevClient(std::string url, int timeout_ms) : _timeout_ms(timeout_ms) {
    KevTransport::Reply error;
    if (!parse_url(url, error, _host, _port, _path)) {
        _url_error = error.error;
    }
}

KevTransport::Reply HttpKevClient::post(const std::string& body) {
    Reply reply;
    if (!_url_error.empty()) {
        reply.error = _url_error;
        return reply;
    }
    if (_host.empty()) {
        reply.error = "invalid Kev URL";
        return reply;
    }

    const std::string port_text = std::to_string(_port);
    const addrinfo hints{
        .ai_flags    = AI_ADDRCONFIG,
        .ai_family   = AF_UNSPEC,
        .ai_socktype = SOCK_STREAM,
        .ai_protocol = 0,
    };
    addrinfo* resolved = nullptr;
    const int lookup =
        ::getaddrinfo(_host.c_str(), port_text.c_str(), &hints, &resolved);
    if (lookup != 0 || resolved == nullptr) {
        reply.error = std::string("could not resolve Kev host: ") + ::gai_strerror(lookup);
        return reply;
    }

    int socket_fd = -1;
    for (const addrinfo* address = resolved; address != nullptr; address = address->ai_next) {
        socket_fd = ::socket(address->ai_family, address->ai_socktype, address->ai_protocol);
        if (socket_fd < 0) {
            continue;
        }

#ifdef SO_NOSIGPIPE
        const int no_sigpipe = 1;
        static_cast<void>(::setsockopt(socket_fd, SOL_SOCKET, SO_NOSIGPIPE, &no_sigpipe,
                                       sizeof(no_sigpipe)));
#endif
        timeval timeout{};
        timeout.tv_sec  = _timeout_ms / 1000;
        timeout.tv_usec = (_timeout_ms % 1000) * 1000;
        static_cast<void>(
            ::setsockopt(socket_fd, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout)));
        static_cast<void>(
            ::setsockopt(socket_fd, SOL_SOCKET, SO_SNDTIMEO, &timeout, sizeof(timeout)));

        if (::connect(socket_fd, address->ai_addr, address->ai_addrlen) == 0) {
            break;
        }
        static_cast<void>(::close(socket_fd));
        socket_fd = -1;
    }
    ::freeaddrinfo(resolved);

    if (socket_fd < 0) {
        reply.error = "could not connect to Kev server at " + _host + ":" + port_text;
        return reply;
    }

    std::string request = "POST " + _path + " HTTP/1.1\r\n";
    request += "Host: " + _host + ":" + port_text + "\r\n";
    request += "Content-Type: application/json\r\n";
    request += "Accept: application/json\r\n";
    request += "Content-Length: " + std::to_string(body.size()) + "\r\n";
    request += "Connection: close\r\n\r\n";
    request += body;

    if (!send_all(socket_fd, request.data(), request.size())) {
        reply.error = "failed to send request to Kev server";
        static_cast<void>(::close(socket_fd));
        return reply;
    }

    std::string response;
    std::string buffer(8192, '\0');
    while (response.size() < kMaxResponseBytes) {
        const ssize_t count = ::recv(socket_fd, buffer.data(), buffer.size(), 0);
        if (count < 0) {
            if (errno == EINTR) {
                continue;
            }
            reply.error = "failed to read response from Kev server";
            break;
        }
        if (count == 0) {
            break;
        }
        response.append(buffer.data(), static_cast<std::size_t>(count));
    }
    static_cast<void>(::close(socket_fd));

    if (!reply.error.empty()) {
        return reply;
    }

    const std::size_t header_end = response.find("\r\n\r\n");
    if (header_end == std::string::npos) {
        reply.error = "malformed HTTP response from Kev server";
        return reply;
    }

    const std::string headers = response.substr(0, header_end);
    const std::size_t first_space = headers.find(' ');
    if (headers.rfind("HTTP/", 0) != 0 || first_space == std::string::npos) {
        reply.error = "malformed HTTP status line from Kev server";
        return reply;
    }
    reply.status = std::atoi(headers.c_str() + first_space + 1);
    reply.ok     = reply.status >= 200 && reply.status < 300;

    std::string payload = response.substr(header_end + 4);
    std::string lower   = headers;
    std::transform(lower.begin(), lower.end(), lower.begin(),
                   [](unsigned char character) { return std::tolower(character); });
    if (lower.find("transfer-encoding: chunked") != std::string::npos) {
        payload = decode_chunked(payload);
    }
    reply.body  = std::move(payload);
    if (!reply.ok && reply.error.empty()) {
        reply.error = "Kev server returned HTTP " + std::to_string(reply.status);
    }
    return reply;
}

} // namespace dungeon::agent
