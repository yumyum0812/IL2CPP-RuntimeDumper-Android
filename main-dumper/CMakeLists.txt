set(LIB_NAME "main-dumper")

add_library(${LIB_NAME} SHARED
        src/lib_main.cpp
)

target_link_libraries(${LIB_NAME} PRIVATE
        dumper
        android
        dl
)

set_target_properties(${LIB_NAME} PROPERTIES
    OUTPUT_NAME "main"
)