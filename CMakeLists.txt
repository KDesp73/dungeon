cmake_minimum_required(VERSION 3.20)
project(dungeon LANGUAGES CXX)

# Set C++ standard
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_executable(dungeon src/main.cpp)
