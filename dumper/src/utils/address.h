#pragma once
#include <cstdint>

uintptr_t NormalizePointer(uintptr_t addr) {
    #ifdef __aarch64__
    addr &= 0x00FFFFFFFFFFFFFF;
    #endif

    return addr;
}