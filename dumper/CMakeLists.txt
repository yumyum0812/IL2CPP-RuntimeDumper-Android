set(LIB_NAME "dumper")

add_library(${LIB_NAME} STATIC
        src/dumper.cpp
)

target_include_directories(${LIB_NAME} PUBLIC include)

target_link_libraries(${LIB_NAME} PRIVATE
        android
        log
        xdl
        dobby
        json
        KittyMemory
)

add_subdirectory(external/xdl)
add_subdirectory(external/dobby)
add_subdirectory(external/json)
add_subdirectory(external/KittyMemory)