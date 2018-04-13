cmake_minimum_required(VERSION 2.8)
project(ClMemdisk)

set(CMAKE_C_FLAGS "-std=c99")

set(SOURCE_FILES ClMemdisk.c)

add_executable(ClMemdisk ${SOURCE_FILES})
target_link_libraries(ClMemdisk OpenCL buse)