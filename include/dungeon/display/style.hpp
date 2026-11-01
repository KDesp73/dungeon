#pragma once

#include <cstdint>

namespace dungeon::display {

/// Colors understood by the terminal renderer.
enum class Color : std::uint8_t {
    Default,
    Black,
    Red,
    Green,
    Yellow,
    Blue,
    Magenta,
    Cyan,
    White,
    BrightBlack,
    BrightRed,
    BrightGreen,
    BrightYellow,
    BrightBlue,
    BrightMagenta,
    BrightCyan,
    BrightWhite,
};

/// Text attributes that can be combined with `operator|`.
enum class TextStyle : std::uint8_t {
    None          = 0,
    Bold          = 1U << 0,
    Dim           = 1U << 1,
    Italic        = 1U << 2,
    Underline     = 1U << 3,
    Blink         = 1U << 4,
    Reverse       = 1U << 5,
    Hidden        = 1U << 6,
    Strikethrough = 1U << 7,
};

constexpr TextStyle operator|(TextStyle lhs, TextStyle rhs) noexcept {
    return static_cast<TextStyle>(static_cast<std::uint8_t>(lhs) |
                                  static_cast<std::uint8_t>(rhs));
}

constexpr TextStyle& operator|=(TextStyle& lhs, TextStyle rhs) noexcept {
    lhs = lhs | rhs;
    return lhs;
}

[[nodiscard]] constexpr bool has_style(TextStyle value, TextStyle flag) noexcept {
    return (static_cast<std::uint8_t>(value) & static_cast<std::uint8_t>(flag)) != 0;
}

/// The foreground color, background color, and text attributes of one cell.
struct CellStyle {
    Color     foreground = Color::Default;
    Color     background = Color::Default;
    TextStyle text       = TextStyle::None;

    friend constexpr bool operator==(const CellStyle&, const CellStyle&) = default;
};

[[nodiscard]] constexpr bool is_default(const CellStyle& style) noexcept {
    return style.foreground == Color::Default && style.background == Color::Default &&
           style.text == TextStyle::None;
}

} // namespace dungeon::display
