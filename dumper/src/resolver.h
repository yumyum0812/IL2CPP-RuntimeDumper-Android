#pragma once
#include <cstdint>
#include <thread>
#include <map>
#include <xdl.h>
#include "api/functions.h"
#include "utils/address.h"
#include "log.h"


class LibResolver {
    inline constexpr static auto LIB_NAME = "libil2cpp.so";
    inline constexpr static uintptr_t ADDR_INVALID = UINTPTR_MAX;

    struct LoadSegment {
        uintptr_t vaddr;
        uintptr_t offset;
        uintptr_t filesz;
    };

    void* handle = nullptr;
    uintptr_t base = 0;
    std::vector<LoadSegment> segments;

    static void* FindHandle() {
        return xdl_open(LIB_NAME, XDL_DEFAULT);
    }

public:
    void Resolve() {
        while (!(handle = FindHandle())) {
            std::this_thread::sleep_for(std::chrono::milliseconds(50));
        }

        xdl_info_t info{};
        xdl_info(handle, XDL_DI_DLINFO, &info);
        base = NormalizePointer((uintptr_t) info.dli_fbase);

        for (size_t i = 0; i < info.dlpi_phnum; ++i) {
            const auto& ph = info.dlpi_phdr[i];
            if (ph.p_type != PT_LOAD || ph.p_filesz == 0)
                continue;

            segments.push_back({
                static_cast<uintptr_t>(ph.p_vaddr),
                static_cast<uintptr_t>(ph.p_offset),
                static_cast<uintptr_t>(ph.p_filesz)
            });
        }
    }

    uintptr_t RvaToOffset(uintptr_t rva) {
        rva = NormalizePointer(rva);
        
        for (const auto& seg : segments) {
            if (rva >= seg.vaddr && rva < seg.vaddr + seg.filesz) {
                return rva - seg.vaddr + seg.offset;
            }
        }
        return ADDR_INVALID;
    }

    uintptr_t GetBaseAddress() {
        return base;
    }

    void* ResolveSymbol(const std::string& name) {
        auto s = xdl_sym(handle, name.c_str(), nullptr);
        if (s) return s;

        return xdl_dsym(handle, name.c_str(), nullptr);
    }
};


class ApiResolver {
public:
    ApiResolver() = default;
    virtual ~ApiResolver() = default;
    virtual void Resolve(LibResolver& lf, const std::vector<Api::Functions::IApiCall*>& calls) = 0;
};


class SymApiResolver : public ApiResolver {
    std::map<std::string, std::string> symbolMap;

public:
    SymApiResolver(const std::map<std::string, std::string>& symbolMap = {}) {
        this->symbolMap = symbolMap;
    }

    void Resolve(LibResolver& lf, const std::vector<Api::Functions::IApiCall*>& calls) override {
        for (auto f : calls) {
            auto funcName = f->Name();

            auto symName = funcName;
            if (this->symbolMap.contains(funcName))
                symName = this->symbolMap.at(funcName);

            void* ptr = lf.ResolveSymbol(symName);
            if (!ptr) {
                Log::Debug("apiresolver: symbol not found for {}: {}", funcName, symName);
                continue;
            }
            f->Assign(ptr);
        }
    }
};

class RvaApiResolver : public ApiResolver {
    std::map<std::string, size_t> offMap;

public:
    RvaApiResolver(const std::map<std::string, size_t>& offMap = {}) {
        this->offMap = offMap;
    }

    void Resolve(LibResolver& lf, const std::vector<Api::Functions::IApiCall*>& calls) override {
        for (auto f : calls) {
            auto funcName = f->Name();
            if (!offMap.contains(funcName)) {
                Log::Debug("apiresolver: offset not found for {}", funcName);
                continue;
            }
            uintptr_t addr = lf.GetBaseAddress() + offMap.at(funcName);
            f->Assign((void*) addr);
        }
    }
};


bool InitApi(LibResolver& libRes, ApiResolver& res) {
    Log::Debug("init: resolving library...");
    libRes.Resolve();
    
    Log::Debug("init: resolving api...");
    res.Resolve(libRes, Api::Functions::_apiCalls);
    
    bool ok = true;
    for (auto f : Api::Functions::_apiCalls) {
        if (!f->Valid()) {
            Log::Debug("init: function not initialized: {}", f->Name());
            ok = false;
        }
    }

    if (!ok) {
        Log::Debug("init: not all needed functions initialized!");
    } else {
        Log::Debug("init: initialized needed functions");
    }

    return ok;
}