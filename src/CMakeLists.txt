cmake_minimum_required(VERSION 3.14 FATAL_ERROR)
project(thread_pool LANGUAGES CXX)

set(CMAKE_BUILD_TYPE "Debug")
set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_FLAGS_DEBUG "-O3 -g -Wall")
set(CMAKE_CXX_FLAGS_RELEASE "-O3 -Wall")

include_directories(${CMAKE_SOURCE_DIR}/include)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_SOURCE_DIR}/libs)

file(GLOB_RECURSE SRC *.cpp)    # 递归添加当前目录中的所有源文件

add_library(Thread
    SHARED
    ${SRC})