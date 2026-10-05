project("dobby-static")

add_library(${PROJECT_NAME} STATIC IMPORTED GLOBAL)

set_target_properties(${PROJECT_NAME} PROPERTIES
    IMPORTED_LOCATION ${PROJECT_SOURCE_DIR}/lib/${CMAKE_ANDROID_ARCH_ABI}/libdobby.a
)

target_include_directories(${PROJECT_NAME} INTERFACE
    include
)

target_link_options(${PROJECT_NAME} INTERFACE "-Wl,--exclude-libs,libdobby.a")