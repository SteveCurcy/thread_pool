cmake_minimum_required(VERSION 3.14 FATAL_ERROR)
project(thread_pool LANGUAGES CXX)

set(CMAKE_BUILD_TYPE "Debug")
set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_FLAGS_DEBUG "-O3 -g -Wall")
set(CMAKE_CXX_FLAGS_RELEASE "-O3 -Wall")

include_directories(include)
link_directories(${CMAKE_LIBRARY_OUTPUT_DIRECTORY})

add_subdirectory(./src)
add_subdirectory(./test)

enable_testing()
add_test(NAME TaskCache COMMAND TaskCache)
add_test(NAME LFQueBatch COMMAND LFQueBatch)