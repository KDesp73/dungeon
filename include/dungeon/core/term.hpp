#pragma once

#ifdef _WIN32
#include <windows.h>
#else
#include <sys/ioctl.h>
#include <unistd.h>
#endif

namespace dungeon::core {

void term_size(int& width, int& height);

} // namespace dungeon::core
