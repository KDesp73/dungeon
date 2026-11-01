#pragma once

#include <cstddef>
#include <string>
#include <string_view>

namespace dungeon::display::utf8 {

/// Appends @p codepoint to @p out as UTF-8.
///
/// Surrogate code points and values above U+10FFFF are replaced with U+FFFD.
void append(std::string& out, char32_t codepoint);

/// Decodes one code point from @p text starting at @p offset.
///
/// Returns the number of bytes consumed and writes the decoded code point to
/// @p codepoint. Invalid input consumes one byte and yields U+FFFD. Returns 0
/// when @p offset is at or past the end of @p text.
[[nodiscard]] std::size_t decode(std::string_view text, std::size_t offset,
                                 char32_t& codepoint) noexcept;

} // namespace dungeon::display::utf8
