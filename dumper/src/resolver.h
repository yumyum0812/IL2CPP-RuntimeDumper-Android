#pragma once
#include <cstdint>
#include <dlfcn.h>
#include <limits>
#include <thread>
#include <map>
#include <xdl.h>
#include "api/functions.h"
#include "utils/address.h"
#include "dumper_log.h"


class LibResolver {
    inline constexpr static uintptr_t INVALID_ADDR = std::numeric_limits<uintptr_t>::max();

    struct LoadSegment {
        uintptr_t vaddr{};
        uintptr_t offset{};
        uintptr_t memsz{};
        uintptr_t filesz{};
    };

    std::string libName = "";
    void* handle = nullptr;
    uintptr_t base = 0;
    std::vector<LoadSegment> segments;

    void* FindHandle() {
        return xdl_open(libName.c_str(), XDL_DEFAULT);
    }

public:
    LibResolver(std::string_view libName = "libil2cpp.so") {
        this->libName = libName;
    }

    bool Resolve() {
        DumperLog::Debug("resolver: resolving library {}", libName);

        int elapsedMs = 0;
        while (!(handle = FindHandle())) {
            if (elapsedMs >= 20'000) {
                DumperLog::Error("resolver: library {} not found for 20 seconds", libName);
                return false;
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(50));
            elapsedMs += 50;
        }

        xdl_info_t info{};
        xdl_info(handle, XDL_DI_DLINFO, &info);
        base = NormalizePointer((uintptr_t) info.dli_fbase);

        for (size_t i = 0; i < info.dlpi_phnum; ++i) {
            const auto& ph = info.dlpi_phdr[i];
            if (ph.p_type != PT_LOAD || !ph.p_memsz)
                continue;

            segments.push_back({
                (uintptr_t) ph.p_vaddr,
                (uintptr_t) ph.p_offset,
                (uintptr_t) ph.p_memsz,
                (uintptr_t) ph.p_filesz,
            });
        }

        return true;
    }

    uintptr_t RvaToOffset(uintptr_t rva) const {
        for (const auto& seg : segments) {
            if (rva >= seg.vaddr && rva < seg.vaddr + seg.memsz) {
                return rva - seg.vaddr + seg.offset;
            }
        }
        return INVALID_ADDR;
    }

    uintptr_t OffsetToRva(uintptr_t offset) const {
        for (const auto& seg : segments) {
            if (offset >= seg.offset && offset < seg.offset + seg.memsz) {
                return offset - seg.offset + seg.vaddr;
            }
        }
        return INVALID_ADDR;
    }

    bool HasRva(uintptr_t rva) const {
        for (const auto& seg : segments) {
            if (rva >= seg.vaddr && rva < seg.vaddr + seg.memsz)
                return true;
        }
        return false;
    }

    uintptr_t GetBaseAddress() const {
        return base;
    }

    void* ResolveSymbol(const std::string& name) const {
        auto s = xdl_sym(handle, name.c_str(), nullptr);
        if (s) return s;

        return xdl_dsym(handle, name.c_str(), nullptr);
    }
};


class ApiResolver {
public:
    std::map<std::string, std::string> symbolMap;
    std::map<std::string, size_t> rvaMap;
    
    ApiResolver() = default;
    
    void Resolve(const LibResolver& lr, const std::vector<Api::IApiCall*>& calls) {
        for (auto f : calls) {
            auto origSym = f->Name();
            if (!rvaMap.contains(origSym))
                continue;

            size_t rva = rvaMap.at(origSym);
            if (!lr.HasRva(rva)) {
                DumperLog::Warn("apiresolver: rva out of range for {}: 0x{:X}", origSym, rva);
                continue;
            }
            uintptr_t addr = lr.GetBaseAddress() + rva;
            f->Assign((void*) addr);
        }

        for (auto f : calls) {
            if (f->Valid())
                continue;

            auto origSym = f->Name();

            auto mappedSym = origSym;
            if (this->symbolMap.contains(origSym))
                mappedSym = this->symbolMap.at(origSym);

            void* ptr = lr.ResolveSymbol(mappedSym);
            if (!ptr) {
                DumperLog::Warn("apiresolver: symbol not found for {}: {}", origSym, mappedSym);
                continue;
            }
            f->Assign(ptr);
        }
    }
};


inline bool InitApi(LibResolver& libRes, ApiResolver& res) {
    DumperLog::Debug("init: resolving library...");
    if (!libRes.Resolve()) {
        DumperLog::Debug("init: failed to resolve library");
    }
    
    DumperLog::Debug("init: resolving api...");
    res.Resolve(libRes, Api::Functions::GetCalls());

    bool ok = true;
    for (auto f : Api::Functions::GetCalls()) {
        if (!f->Valid()) {
            DumperLog::Error("init: function not initialized: {}", f->Name());
            ok = false;
        }
    }

    if (!ok) {
        DumperLog::Error("init: not all needed functions initialized!");
    } else {
        DumperLog::Debug("init: initialized needed functions");
    }

    return ok;
}