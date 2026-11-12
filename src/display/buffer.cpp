#include "dungeon/display/buffer.hpp"

#include "dungeon/display/utf8.hpp"

#ifdef __EMSCRIPTEN__
#include <emscripten/emscripten.h>
#endif

#include <algorithm>
#include <cstddef>
#include <cstdio>
#include <string>
#include <string_view>
#include <utility>

namespace dungeon::display {

namespace {

constexpr int foreground_code(Color color) noexcept {
    switch (color) {
    case Color::Black:
        return 30;
    case Color::Red:
        return 31;
    case Color::Green:
        return 32;
    case Color::Yellow:
        return 33;
    case Color::Blue:
        return 34;
    case Color::Magenta:
        return 35;
    case Color::Cyan:
        return 36;
    case Color::White:
        return 37;
    case Color::BrightBlack:
        return 90;
    case Color::BrightRed:
        return 91;
    case Color::BrightGreen:
        return 92;
    case Color::BrightYellow:
        return 93;
    case Color::BrightBlue:
        return 94;
    case Color::BrightMagenta:
        return 95;
    case Color::BrightCyan:
        return 96;
    case Color::BrightWhite:
        return 97;
    case Color::Default:
        return 39;
    }
    return 39;
}

constexpr int background_code(Color color) noexcept {
    switch (color) {
    case Color::Black:
        return 40;
    case Color::Red:
        return 41;
    case Color::Green:
        return 42;
    case Color::Yellow:
        return 43;
    case Color::Blue:
        return 44;
    case Color::Magenta:
        return 45;
    case Color::Cyan:
        return 46;
    case Color::White:
        return 47;
    case Color::BrightBlack:
        return 100;
    case Color::BrightRed:
        return 101;
    case Color::BrightGreen:
        return 102;
    case Color::BrightYellow:
        return 103;
    case Color::BrightBlue:
        return 104;
    case Color::BrightMagenta:
        return 105;
    case Color::BrightCyan:
        return 106;
    case Color::BrightWhite:
        return 107;
    case Color::Default:
        return 49;
    }
    return 49;
}

void append_parameter(std::string& out, int parameter) {
    if (!out.empty()) {
        out.push_back(';');
    }
    out += std::to_string(parameter);
}

/// Appends the SGR sequence that switches the terminal to @p style.
///
/// A reset is always emitted first so a previous cell's attributes never leak
/// into the new one.
void append_style(std::string& out, CellStyle style) {
    out += "\033[0m";

    if (is_default(style)) {
        return;
    }

    std::string parameters;
    if (has_style(style.text, TextStyle::Bold)) {
        append_parameter(parameters, 1);
    }
    if (has_style(style.text, TextStyle::Dim)) {
        append_parameter(parameters, 2);
    }
    if (has_style(style.text, TextStyle::Italic)) {
        append_parameter(parameters, 3);
    }
    if (has_style(style.text, TextStyle::Underline)) {
        append_parameter(parameters, 4);
    }
    if (has_style(style.text, TextStyle::Blink)) {
        append_parameter(parameters, 5);
    }
    if (has_style(style.text, TextStyle::Reverse)) {
        append_parameter(parameters, 7);
    }
    if (has_style(style.text, TextStyle::Hidden)) {
        append_parameter(parameters, 8);
    }
    if (has_style(style.text, TextStyle::Strikethrough)) {
        append_parameter(parameters, 9);
    }
    if (style.foreground != Color::Default) {
        append_parameter(parameters, foreground_code(style.foreground));
    }
    if (style.background != Color::Default) {
        append_parameter(parameters, background_code(style.background));
    }

    out += "\033[";
    out += parameters;
    out.push_back('m');
}

} // namespace

char FrameBuffer::at(std::size_t x, std::size_t y) const noexcept {
    const char32_t glyph = glyph_at(x, y);
    if (glyph <= 0x7F) {
        return static_cast<char>(glyph);
    }
    return '?';
}

char32_t FrameBuffer::glyph_at(std::size_t x, std::size_t y) const noexcept {
    return cell_at(x, y).glyph;
}

const Cell& FrameBuffer::cell_at(std::size_t x, std::size_t y) const noexcept {
    static constexpr Cell kEmpty{};

    if (x >= _width || y >= _height) {
        return kEmpty;
    }
    return _back_buffer[y * _width + x];
}

void FrameBuffer::draw(std::size_t x, std::size_t y, char c) {
    draw(x, y, static_cast<char32_t>(static_cast<unsigned char>(c)), CellStyle{});
}

void FrameBuffer::draw(std::size_t x, std::size_t y, char32_t c) {
    draw(x, y, c, CellStyle{});
}

void FrameBuffer::draw(std::size_t x, std::size_t y, char c, CellStyle style) {
    draw(x, y, static_cast<char32_t>(static_cast<unsigned char>(c)), style);
}

void FrameBuffer::draw(std::size_t x, std::size_t y, char32_t c, CellStyle style) {
    if (x >= _width || y >= _height) {
        return;
    }

    Cell& cell = _back_buffer[y * _width + x];
    cell.glyph = c;
    cell.style = style;
}

std::string FrameBuffer::render() const {
    std::string frame;
    frame.reserve(_width * _height + 16);

    frame += "\033[H";

    CellStyle current{};
    for (std::size_t y = 0; y < _height; ++y) {
        for (std::size_t x = 0; x < _width; ++x) {
            const Cell& cell = _back_buffer[y * _width + x];

            if (!(cell.style == current)) {
                append_style(frame, cell.style);
                current = cell.style;
            }

            utf8::append(frame, cell.glyph);
        }

        if (y + 1 < _height) {
            frame.push_back('\n');
        }
    }

    if (!is_default(current)) {
        frame += "\033[0m";
    }

    return frame;
}

void FrameBuffer::present() {
    const std::string frame = render();

    std::swap(_back_buffer, _front_buffer);

#ifdef __EMSCRIPTEN__
    // Hand the rendered ANSI frame to the browser shell, which writes it into
    // xterm.js. Writing to stdout would only reach the developer console.
    // EM_ASM's `$0` placeholders are valid, but Clang warns about the dollar
    // sign in an otherwise C++ translation unit.
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wdollar-in-identifier-extension"
    EM_ASM(
        {
            if (typeof Module !== 'undefined' && Module.dungeonWriteFrame) {
                Module.dungeonWriteFrame(UTF8ToString($0, $1));
            }
        },
        frame.data(), static_cast<int>(frame.size()));
#pragma clang diagnostic pop
#else
    std::fwrite(frame.data(), sizeof(char), frame.size(), stdout);
    std::fflush(stdout);
#endif

    std::fill(_back_buffer.begin(), _back_buffer.end(), Cell{});
}

} // namespace dungeon::display
