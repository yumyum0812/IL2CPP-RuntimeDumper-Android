cmake_minimum_required(VERSION 3.16)
project(KittyMemory VERSION 1.0 LANGUAGES CXX)


option(KT_DISABLE_KEYSTONE "Disable keystone support" OFF)


set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)


file(GLOB KITTYMEMORY_SRC "${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp")

add_library(${PROJECT_NAME} STATIC ${KITTYMEMORY_SRC})


target_include_directories(${PROJECT_NAME} PUBLIC include)
target_compile_definitions(${PROJECT_NAME} PRIVATE
    $<$<CONFIG:Debug>:DEBUG>
    $<$<CONFIG:Release>:NDEBUG>
    $<$<BOOL:${KT_DISABLE_KEYSTONE}>:kNO_KEYSTONE>
)

target_compile_options(${PROJECT_NAME} PRIVATE
    $<$<CONFIG:Debug>:-O0 -g>
    $<$<CONFIG:Release>:-O2>
    -Wall
    -Wextra
    -Werror
    -fexceptions
    -Wno-unused-command-line-argument
)

target_link_libraries(${PROJECT_NAME} PRIVATE log $<$<NOT:$<BOOL:${KT_DISABLE_KEYSTONE}>>:Keystone>)


add_subdirectory(external/Keystone)