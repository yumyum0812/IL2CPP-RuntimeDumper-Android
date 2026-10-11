set(LIB_NAME "dumper-module")

add_library(${LIB_NAME} SHARED
    src/lib_main.cpp
)

target_link_libraries(${LIB_NAME} PRIVATE
    android
    dl
    dumper-core
)

target_include_directories(${LIB_NAME} PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)