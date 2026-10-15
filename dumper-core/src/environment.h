#pragma once

#if defined(__aarch64__) || defined(_M_ARM64)
    inline constexpr auto ARCH_NAME = "arm64-v8a";
#elif defined(__arm__) || defined(_M_ARM)
    inline constexpr auto ARCH_NAME = "armeabi-v7a";
#else
    #error "Unsupported architecture"
#endif