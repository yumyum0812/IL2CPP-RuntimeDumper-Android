set(LIB_NAME "fast-dumper")

add_library(${LIB_NAME} SHARED
        src/lib_main.cpp
)

target_link_libraries(${LIB_NAME} PRIVATE
        dumper
)