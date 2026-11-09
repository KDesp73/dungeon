#include "dungeon/controls.hpp"

#include <optional>

#ifdef _WIN32
#include <conio.h>
#elif defined(__EMSCRIPTEN__)
#include <emscripten/emscripten.h>

#include <deque>
#else
#include <poll.h>
#include <termios.h>
#include <unistd.h>
#endif

namespace dungeon::controls {

struct TerminalSession::Impl {
    bool active = false;

#if !defined(_WIN32) && !defined(__EMSCRIPTEN__)
    struct termios previous{};
#endif
};

#ifdef _WIN32

namespace {

KeyEvent get_key_impl() {
    const int code = _getch();

    if (code == 0 || code == 0xE0) {
        switch (_getch()) {
        case 72:
            return KeyEvent::from_kind(KeyKind::Up);
        case 80:
            return KeyEvent::from_kind(KeyKind::Down);
        case 75:
            return KeyEvent::from_kind(KeyKind::Left);
        case 77:
            return KeyEvent::from_kind(KeyKind::Right);
        default:
            return {};
        }
    }

    if (code == 27) {
        return KeyEvent::from_kind(KeyKind::Escape);
    }

    return KeyEvent::from_character(static_cast<char>(code));
}

} // namespace

TerminalSession::TerminalSession() : _impl(std::make_unique<Impl>()) {
    // _getch is already unbuffered and does not echo, so there is nothing to
    // change. Keep the session object so callers have one lifetime to manage.
    _impl->active = true;
}

TerminalSession::~TerminalSession() = default;

bool TerminalSession::active() const noexcept {
    return _impl->active;
}

KeyEvent get_key() {
    return get_key_impl();
}

KeyEvent poll_key() {
    if (!_kbhit()) {
        return {};
    }
    return get_key_impl();
}

#elif defined(__EMSCRIPTEN__)

namespace {

/// Keys pushed by the browser shell, waiting to be consumed by the game loop.
std::deque<KeyEvent> g_key_queue;

/// Builds a key event from the browser's `(kind, value)` encoding.
///
/// Kind `0` is a printable character and carries its code point in `value`.
/// Kinds `1` to `5` are the non-character keys the game understands.
KeyEvent key_from_browser(int kind, int value) {
    switch (kind) {
    case 1:
        return KeyEvent::from_kind(KeyKind::Up);
    case 2:
        return KeyEvent::from_kind(KeyKind::Down);
    case 3:
        return KeyEvent::from_kind(KeyKind::Left);
    case 4:
        return KeyEvent::from_kind(KeyKind::Right);
    case 5:
        return KeyEvent::from_kind(KeyKind::Escape);
    default:
        break;
    }

    return KeyEvent::from_character(static_cast<char>(value));
}

} // namespace

extern "C" {

/// Queues a key press sent by the JavaScript shell.
///
/// Called as `dungeon_push_key(kind, value)`. Non-character kinds are 1 = Up,
/// 2 = Down, 3 = Left, 4 = Right, and 5 = Escape; every other kind is treated
/// as a printable character whose code point is @p value.
EMSCRIPTEN_KEEPALIVE void dungeon_push_key(int kind, int value) {
    g_key_queue.push_back(key_from_browser(kind, value));
}

} // extern "C"

TerminalSession::TerminalSession() : _impl(std::make_unique<Impl>()) {
    // The browser owns the keyboard; there is no terminal mode to switch.
    _impl->active = true;
}

TerminalSession::~TerminalSession() = default;

bool TerminalSession::active() const noexcept {
    return _impl->active;
}

KeyEvent get_key() {
    if (g_key_queue.empty()) {
        return {};
    }

    const KeyEvent key = g_key_queue.front();
    g_key_queue.pop_front();
    return key;
}

KeyEvent poll_key() {
    return get_key();
}

#else

namespace {

/// Bytes read past the first one while decoding a sequence, kept for the next
/// poll so a lone Escape followed by a quick key press does not swallow it.
std::optional<char> g_pending;

/// Reads one byte, waiting at most @p timeout_ms milliseconds.
///
/// A negative timeout blocks until a byte arrives.
bool read_byte(int timeout_ms, char& out) {
    if (g_pending) {
        out = *g_pending;
        g_pending.reset();
        return true;
    }

    struct pollfd descriptor{};
    descriptor.fd     = STDIN_FILENO;
    descriptor.events = POLLIN;

    if (poll(&descriptor, 1, timeout_ms) <= 0) {
        return false;
    }

    return read(STDIN_FILENO, &out, 1) == 1;
}

/// Decodes an ANSI escape sequence after its leading ESC byte.
KeyEvent decode_escape_sequence() {
    char introducer = 0;
    if (!read_byte(10, introducer)) {
        return KeyEvent::from_kind(KeyKind::Escape);
    }

    if (introducer != '[' && introducer != 'O') {
        // Not a sequence: the user pressed Escape and then another key. Keep
        // that key for the next poll instead of dropping it.
        g_pending = introducer;
        return KeyEvent::from_kind(KeyKind::Escape);
    }

    // Consume parameter/intermediate bytes until the final byte. This also
    // handles modified arrows such as `ESC [ 1 ; 2 A`.
    for (int i = 0; i < 8; ++i) {
        char final = 0;
        if (!read_byte(10, final)) {
            return {};
        }

        switch (final) {
        case 'A':
            return KeyEvent::from_kind(KeyKind::Up);
        case 'B':
            return KeyEvent::from_kind(KeyKind::Down);
        case 'C':
            return KeyEvent::from_kind(KeyKind::Right);
        case 'D':
            return KeyEvent::from_kind(KeyKind::Left);
        default:
            if (final >= 0x40 && final <= 0x7E) {
                // A complete but unrecognised sequence, for example Page Up.
                return {};
            }
            break;
        }
    }

    return {};
}

/// Decodes the first byte of a key press.
KeyEvent decode_key(char first) {
    if (first == 0x1B) {
        return decode_escape_sequence();
    }

    return KeyEvent::from_character(first);
}

} // namespace

TerminalSession::TerminalSession() : _impl(std::make_unique<Impl>()) {
    if (tcgetattr(STDIN_FILENO, &_impl->previous) < 0) {
        return;
    }

    struct termios raw = _impl->previous;
    raw.c_lflag &= static_cast<tcflag_t>(~(ICANON | ECHO));
    raw.c_cc[VMIN]  = 1;
    raw.c_cc[VTIME] = 0;

    if (tcsetattr(STDIN_FILENO, TCSANOW, &raw) < 0) {
        return;
    }

    _impl->active = true;
}

TerminalSession::~TerminalSession() {
    if (_impl->active) {
        tcsetattr(STDIN_FILENO, TCSADRAIN, &_impl->previous);
    }
}

bool TerminalSession::active() const noexcept {
    return _impl->active;
}

KeyEvent get_key() {
    char first = 0;
    if (!read_byte(-1, first)) {
        return {};
    }
    return decode_key(first);
}

KeyEvent poll_key() {
    char first = 0;
    if (!read_byte(0, first)) {
        return {};
    }
    return decode_key(first);
}

#endif

} // namespace dungeon::controls
