cmake_minimum_required(VERSION 3.14 FATAL_ERROR)
project(thread_pool LANGUAGES CXX)

set(CMAKE_BUILD_TYPE "Debug")
set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_FLAGS_DEBUG "-O3 -g -Wall")
set(CMAKE_CXX_FLAGS_RELEASE "-O3 -Wall")

include_directories(${CMAKE_CURRENT_SOURCE_DIR}/include)
link_directories(${CMAKE_SOURCE_DIR}/libs)

add_executable(TaskCache TaskCache.cpp)
target_link_libraries(TaskCache Thread pthread)

add_executable(LFQueBatch LFQueBatch.cpp)
target_link_libraries(LFQueBatch Thread pthread)