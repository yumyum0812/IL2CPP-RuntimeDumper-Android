cmake_minimum_required(VERSION 4.0)

project(RuntimeDumper)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

add_link_options($<$<CONFIG:Release>:-s>)

add_subdirectory(dumper-core)
add_subdirectory(dumper-inject)
add_subdirectory(dumper-proxy)
add_subdirectory(dumper-module)