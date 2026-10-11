set(LIB_NAME "dumper-inject")

add_library(${LIB_NAME} SHARED
        src/lib_main.cpp
)

target_link_libraries(${LIB_NAME} PRIVATE
        dumper-core
)