#include "dungeon/display/term.hpp"

namespace dungeon::display::tui {

void term_size(int& width, int& height)
{
#ifdef _WIN32
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

} // namespace dungeon::core
