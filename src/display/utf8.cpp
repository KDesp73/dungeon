#include "dungeon/display/utf8.hpp"

#include <cstdint>

namespace dungeon::display::utf8 {

namespace {

constexpr char32_t kReplacement = 0xFFFD;

constexpr bool is_surrogate(char32_t codepoint) noexcept {
    return codepoint >= 0xD800 && codepoint <= 0xDFFF;
}

} // namespace

void append(std::string& out, char32_t codepoint) {
    if (codepoint > 0x10FFFF || is_surrogate(codepoint)) {
        codepoint = kReplacement;
    }

    if (codepoint <= 0x7F) {
        out.push_back(static_cast<char>(codepoint));
    } else if (codepoint <= 0x7FF) {
        out.push_back(static_cast<char>(0xC0U | (codepoint >> 6)));
        out.push_back(static_cast<char>(0x80U | (codepoint & 0x3FU)));
    } else if (codepoint <= 0xFFFF) {
        out.push_back(static_cast<char>(0xE0U | (codepoint >> 12)));
        out.push_back(static_cast<char>(0x80U | ((codepoint >> 6) & 0x3FU)));
        out.push_back(static_cast<char>(0x80U | (codepoint & 0x3FU)));
    } else {
        out.push_back(static_cast<char>(0xF0U | (codepoint >> 18)));
        out.push_back(static_cast<char>(0x80U | ((codepoint >> 12) & 0x3FU)));
        out.push_back(static_cast<char>(0x80U | ((codepoint >> 6) & 0x3FU)));
        out.push_back(static_cast<char>(0x80U | (codepoint & 0x3FU)));
    }
}

std::size_t decode(std::string_view text, std::size_t offset, char32_t& codepoint) noexcept {
    if (offset >= text.size()) {
        return 0;
    }

    const auto byte = [text](std::size_t index) {
        return static_cast<std::uint8_t>(text[index]);
    };

    const std::uint8_t first = byte(offset);
    if (first < 0x80U) {
        codepoint = first;
        return 1;
    }

    std::size_t length = 0;
    char32_t    value  = 0;

    if ((first & 0xE0U) == 0xC0U) {
        length = 2;
        value  = first & 0x1FU;
    } else if ((first & 0xF0U) == 0xE0U) {
        length = 3;
        value  = first & 0x0FU;
    } else if ((first & 0xF8U) == 0xF0U) {
        length = 4;
        value  = first & 0x07U;
    } else {
        codepoint = kReplacement;
        return 1;
    }

    if (offset + length > text.size()) {
        codepoint = kReplacement;
        return 1;
    }

    for (std::size_t i = 1; i < length; ++i) {
        const std::uint8_t next = byte(offset + i);
        if ((next & 0xC0U) != 0x80U) {
            codepoint = kReplacement;
            return 1;
        }
        value = (value << 6) | (next & 0x3FU);
    }

    const bool overlong = (length == 2 && value < 0x80U) || (length == 3 && value < 0x800U) ||
                          (length == 4 && value < 0x10000U);

    if (overlong || value > 0x10FFFF || is_surrogate(value)) {
        codepoint = kReplacement;
        return 1;
    }

    codepoint = value;
    return length;
}

} // namespace dungeon::display::utf8
