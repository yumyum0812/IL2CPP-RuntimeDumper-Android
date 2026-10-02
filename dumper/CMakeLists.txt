project(dumper)

add_library(dumper SHARED
        src/lib_main.cpp
        src/dumper.cpp
)

target_link_libraries(dumper
        android
        log
        xdl-static
)