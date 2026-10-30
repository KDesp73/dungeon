#include "dungeon/controls.hpp"

#ifdef _WIN32
#include <conio.h>
#else
#include <termios.h>
#include <unistd.h>
#include <cstdio>
#endif

namespace dungeon::controls {

#ifdef _WIN32
char get_key() {
    return static_cast<char>(_getch()); // Unbuffered, no Enter required on Windows
}
#else
char get_key() {
    char buf = 0;
    struct termios old_t{};

    // Get current terminal settings
    if (tcgetattr(STDIN_FILENO, &old_t) < 0) {
        return 0;
    }

    struct termios new_t = old_t;

    // Disable Canonical Mode and Echo (using c_lflag for POSIX/macOS compliance)
    new_t.c_lflag &= static_cast<tcflag_t>(~(ICANON | ECHO));

    // Apply new settings immediately
    if (tcsetattr(STDIN_FILENO, TCSANOW, &new_t) < 0) {
        return 0;
    }

    // Read a single character
    if (read(STDIN_FILENO, &buf, 1) < 0) {
        buf = 0;
    }

    // Restore original terminal settings (using TCSADRAIN on POSIX)
    if (tcsetattr(STDIN_FILENO, TCSADRAIN, &old_t) < 0) {
        return 0;
    }

    return buf;
}
#endif

} // namespace dungeon::controls
