cmake_minimum_required(VERSION 4.0)

project(RuntimeDumper)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

add_subdirectory(dumper)
add_subdirectory(xdl-static)
add_subdirectory(dobby-static)
add_subdirectory(json)