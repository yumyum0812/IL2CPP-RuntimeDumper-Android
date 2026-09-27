#pragma once

#include <filesystem>
#include <cstdint>
#include <link.h>
#include <dlfcn.h>


class LibFinder {
    struct IterArgs {
        std::string_view targetName;
        uintptr_t* baseOut;
    };

    static int IterCallback(dl_phdr_info* info, size_t size, void* userdata) {
        auto* args = reinterpret_cast<IterArgs*>(userdata);

        std::filesystem::path path = info->dlpi_name;
        uintptr_t base = info->dlpi_addr;

        if (path.filename() == args->targetName) {
            *args->baseOut = base;
            return 1;
        }

        return 0;
    }
    
public:
    LibFinder() = delete;

    static bool IsLibraryLoaded(std::string_view name) {
        return TryFind(name) != 0;
    }
    
    static uintptr_t TryFind(std::string_view name) {
        uintptr_t baseOut = 0;
        IterArgs args = {
            .targetName = name,
            .baseOut = &baseOut,
        };

        dl_iterate_phdr(IterCallback, &args);

        return baseOut;
    }
};