#pragma once

#include <memory>

namespace dungeon::controls {

/// The logical meaning of a key press.
enum class KeyKind : unsigned char {
    None,
    Character,
    Up,
    Down,
    Left,
    Right,
    Escape,
};

/// A single decoded key press.
///
/// Arrow keys arrive as multi-byte escape sequences on POSIX terminals and as
/// extended scan codes on Windows. Decoding them here means callers only ever
/// deal with logical keys, never with the bytes the terminal sent.
struct KeyEvent {
    KeyKind kind  = KeyKind::None;
    char    value = '\0';

    [[nodiscard]] static constexpr KeyEvent from_character(char character) noexcept {
        return {KeyKind::Character, character};
    }

    [[nodiscard]] static constexpr KeyEvent from_kind(KeyKind key_kind) noexcept {
        return {key_kind, '\0'};
    }

    [[nodiscard]] constexpr bool is_none() const noexcept { return kind == KeyKind::None; }

    friend constexpr bool operator==(const KeyEvent&, const KeyEvent&) = default;
};

/// Keeps the terminal in raw, no-echo mode for its lifetime.
///
/// Create one around the game loop instead of toggling the terminal mode on
/// every poll. That keeps typed keys from being echoed and scrolled at the
/// bottom of the screen. The previous terminal settings are restored by the
/// destructor.
class TerminalSession {
  public:
    TerminalSession();
    ~TerminalSession();

    TerminalSession(const TerminalSession&)            = delete;
    TerminalSession& operator=(const TerminalSession&) = delete;
    TerminalSession(TerminalSession&&)                 = delete;
    TerminalSession& operator=(TerminalSession&&)      = delete;

    /// True when the terminal was switched into raw mode.
    [[nodiscard]] bool active() const noexcept;

  private:
    struct Impl;
    std::unique_ptr<Impl> _impl;
};

/// Blocks until a key is pressed and returns it.
KeyEvent get_key();

/// Returns the next key when one is already waiting, or a `None` event
/// otherwise.
///
/// This never blocks, so it can be called from a fixed-timestep loop. Arrow
/// key escape sequences are decoded into their logical direction.
KeyEvent poll_key();

} // namespace dungeon::controls
