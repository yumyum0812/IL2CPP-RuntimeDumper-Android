#pragma once
#include <cstdint>

inline uintptr_t NormalizePointer(uintptr_t addr) {
    #ifdef __aarch64__
    addr &= 0x00FFFFFFFFFFFFFF;
    #endif

    return addr;
}