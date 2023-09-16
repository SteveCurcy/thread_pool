cmake_minimum_required(VERSION 3.14 FATAL_ERROR)
project(thread_pool LANGUAGES CXX)

set(CMAKE_BUILD_TYPE "Debug")
set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_COMPILER "/usr/bin/clang++")
set(CMAKE_CXX_FLAGS_DEBUG "-g -Wall")
set(CMAKE_CXX_FLAGS_RELEASE "-O2 -Wall")

include_directories(include)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_SOURCE_DIR}/libs)
link_directories(${CMAKE_LIBRARY_OUTPUT_DIRECTORY})

add_subdirectory(./src)
add_subdirectory(./test)

enable_testing()