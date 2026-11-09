#include "dungeon/display/term.hpp"
#include <cstdio>

namespace dungeon::display::tui {

namespace {

#ifdef __EMSCRIPTEN__
/// The browser terminal has no size to query, so the game renders at a fixed
/// grid. The shell creates its xterm.js instance with the same dimensions.
constexpr int kWebWidth  = 100;
constexpr int kWebHeight = 30;
#endif

} // namespace

void term_size(int& width, int& height)
{
#if defined(__EMSCRIPTEN__)
    width  = kWebWidth;
    height = kWebHeight;
#elif defined(_WIN32)
    CONSOLE_SCREEN_BUFFER_INFO csbi;
    if (GetConsoleScreenBufferInfo(GetStdHandle(STD_OUTPUT_HANDLE), &csbi)) {
        width = csbi.srWindow.Right - csbi.srWindow.Left + 1;
        height = csbi.srWindow.Bottom - csbi.srWindow.Top + 1;
    } else {
        width = 80;  // Default fallback
        height = 24;
    }
#else
    struct winsize w;
    if (ioctl(STDOUT_FILENO, TIOCGWINSZ, &w) == 0 && w.ws_col > 0 && w.ws_row > 0) {
        width = w.ws_col;
        height = w.ws_row;
    } else {
        width = 80;  // Default fallback
        height = 24;
    }
#endif
}

void hide_cursor() {
#if !defined(__EMSCRIPTEN__)
    std::fputs("\033[?25l", stdout);
#endif
}

void show_cursor() {
#if !defined(__EMSCRIPTEN__)
    std::fputs("\033[?25h", stdout);
#endif
}

} // namespace dungeon::display::tui
