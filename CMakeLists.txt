cmake_minimum_required(VERSION 3.14 FATAL_ERROR)
project(thread_pool LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 11)

include_directories(include)
link_directories(${CMAKE_LIBRARY_OUTPUT_DIRECTORY})

add_subdirectory(./src)
add_subdirectory(./test)

enable_testing()
add_test(NAME TaskCache COMMAND TaskCache)