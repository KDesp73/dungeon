#pragma once

#include <string>

namespace dungeon::agent {

/// Minimal HTTP interface so tests can inject a fake Kev endpoint.
class KevTransport {
  public:
    struct Reply {
        bool        ok     = false;
        int         status = 0;
        std::string body;
        std::string error;
    };

    virtual ~KevTransport() = default;

    /// Blocking POST of a JSON request body.
    [[nodiscard]] virtual Reply post(const std::string& body) = 0;
};

/// Small POSIX HTTP/1.1 client for a local Kev server.
///
/// No TLS: the documented endpoint is plain HTTP on loopback. The client is
/// compiled only for native builds.
class HttpKevClient final : public KevTransport {
  public:
    explicit HttpKevClient(std::string url, int timeout_ms = 15000);

    [[nodiscard]] Reply post(const std::string& body) override;

    [[nodiscard]] const std::string& host() const noexcept { return _host; }
    [[nodiscard]] unsigned short port() const noexcept { return _port; }
    [[nodiscard]] const std::string& path() const noexcept { return _path; }

  private:
    std::string    _host;
    unsigned short _port = 80;
    std::string    _path = "/v1/systemone";
    std::string    _url_error;
    int            _timeout_ms;
};

} // namespace dungeon::agent
