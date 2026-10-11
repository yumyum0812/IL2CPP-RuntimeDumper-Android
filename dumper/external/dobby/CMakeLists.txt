set(LIB_NAME "dobby")

add_library(${LIB_NAME} STATIC IMPORTED GLOBAL)

set_target_properties(${LIB_NAME} PROPERTIES
    IMPORTED_LOCATION ${CMAKE_CURRENT_SOURCE_DIR}/lib/${CMAKE_ANDROID_ARCH_ABI}/libdobby.a
)

target_include_directories(${LIB_NAME} INTERFACE
    include
)