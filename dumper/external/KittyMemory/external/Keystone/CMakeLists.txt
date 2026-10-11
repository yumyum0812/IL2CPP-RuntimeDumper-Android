add_library(Keystone STATIC IMPORTED GLOBAL)

set_target_properties(Keystone PROPERTIES
    IMPORTED_LOCATION ${CMAKE_CURRENT_LIST_DIR}/lib/${CMAKE_ANDROID_ARCH_ABI}/libkeystone.a
)

target_include_directories(Keystone INTERFACE
    ${CMAKE_CURRENT_LIST_DIR}/include
)