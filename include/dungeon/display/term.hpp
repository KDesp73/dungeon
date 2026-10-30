#pragma once

#include <cstdio>
#ifdef _WIN32
#include <windows.h>
#else
#include <sys/ioctl.h>
#include <unistd.h>
#endif

namespace dungeon::display::tui {

void term_size(int& width, int& height);


void hide_cursor();
void show_cursor();

} // namespace dungeon::core
