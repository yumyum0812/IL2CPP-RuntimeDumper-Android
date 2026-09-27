#pragma once

#include <chrono>
#include <cstddef>
#include <thread>
#include <xdl.h>
#include "../log.h"


namespace Dumper::Api::Functions {
    struct ApiFuncBase {
        virtual ~ApiFuncBase() = default;

        virtual bool valid() const = 0;
        virtual std::string_view name() const = 0;
    };

    template <typename R, typename... A>
    struct ApiFunc;

    template <typename R, typename... A>
    class ApiFunc<R(A...)> : public ApiFuncBase {
        const std::string _name;
        mutable R (* fn)(A...) = nullptr;
        
    public:
        consteval ApiFunc(std::string_view name): _name(name) {}

        bool valid() const override {
            return fn != nullptr;
        }

        std::string_view name() const override {
            return _name;
        }

        R operator() (A... a) {
            return fn(a...);
        }

        ApiFunc& operator=(R (*ptr)(A...)) {
            fn = ptr;
            return *this;
        }
    };

    ApiFunc<void(int a)> il2cpp_domain_get2("il2cpp_domain_get");

    void* (* il2cpp_domain_get)();
    void** (* il2cpp_domain_get_assemblies)(void* domain, size_t* out);

    void* (* il2cpp_assembly_get_image)(void* assembly);

    void* (* il2cpp_thread_attach)(void* domain);
    void (* il2cpp_thread_detach)(void* thread);

    const char* (* il2cpp_image_get_name)(void* image);
    size_t (* il2cpp_image_get_class_count)(void* image);
    void* (* il2cpp_image_get_class)(void* image, size_t index);

    const char* (* il2cpp_class_get_namespace)(void* klass);
    const char* (* il2cpp_class_get_name)(void* klass);
    void* (* il2cpp_class_get_fields)(void* klass, void** iter);
    void* (* il2cpp_class_get_methods)(void* klass, void** iter);

    const char* (* il2cpp_field_get_name)(void* field);

    const char*   (* il2cpp_method_get_name)(void* method);

    void* (* il2cpp_get_corlib)();


    class Resolver {
    public:
        Resolver() = default;
        virtual void Resolve() = 0;
    };

    class SymResolver : public Resolver {
        void* handle = nullptr;

        static void* GetHandle() {
            return xdl_open("libil2cpp.so", XDL_DEFAULT);
        }

        template<typename F>
        bool Sym(F*& target, const char* name) {
            target = (F*) xdl_sym(handle, name, nullptr);
            if (!target) {
                Log::Debug("resolver: symbol not found: {}", name);
            }
            return target != nullptr;
        }

    public:
        void Resolve() override {
            while (!(handle = GetHandle())) {
                std::this_thread::sleep_for(std::chrono::milliseconds(20));
            }

            Sym(il2cpp_domain_get, "il2cpp_domain_get");
            Sym(il2cpp_domain_get_assemblies, "il2cpp_domain_get_assemblies");
            Sym(il2cpp_assembly_get_image, "il2cpp_assembly_get_image");
            Sym(il2cpp_thread_attach, "il2cpp_thread_attach");
            Sym(il2cpp_thread_detach, "il2cpp_thread_detach");
            Sym(il2cpp_image_get_name, "il2cpp_image_get_name");
            Sym(il2cpp_image_get_class_count, "il2cpp_image_get_class_count");
            Sym(il2cpp_image_get_class, "il2cpp_image_get_class");
            Sym(il2cpp_class_get_namespace, "il2cpp_class_get_namespace");
            Sym(il2cpp_class_get_name, "il2cpp_class_get_name");
            Sym(il2cpp_class_get_fields, "il2cpp_class_get_fields");
            Sym(il2cpp_class_get_methods, "il2cpp_class_get_methods");
            Sym(il2cpp_field_get_name, "il2cpp_field_get_name");
            Sym(il2cpp_method_get_name, "il2cpp_method_get_name");
            Sym(il2cpp_get_corlib, "il2cpp_get_corlib");
        }
    };


    template <typename T>
    bool _CheckPtr(T* ptr, const char* name) {
        if (!ptr) {
            Log::Debug("init: function not initialized: {}", name);
            return false;
        }
        return true;
    }

    bool InitAPI(Resolver& res) {
        Log::Debug("init: resolving...");

        res.Resolve();

        bool ok = true;
        ok &= _CheckPtr(il2cpp_domain_get, "il2cpp_domain_get");
        ok &= _CheckPtr(il2cpp_domain_get_assemblies, "il2cpp_domain_get_assemblies");
        ok &= _CheckPtr(il2cpp_assembly_get_image, "il2cpp_assembly_get_image");
        ok &= _CheckPtr(il2cpp_thread_attach, "il2cpp_thread_attach");
        ok &= _CheckPtr(il2cpp_thread_detach, "il2cpp_thread_detach");
        ok &= _CheckPtr(il2cpp_image_get_name, "il2cpp_image_get_name");
        ok &= _CheckPtr(il2cpp_image_get_class_count, "il2cpp_image_get_class_count");
        ok &= _CheckPtr(il2cpp_image_get_class, "il2cpp_image_get_class");
        ok &= _CheckPtr(il2cpp_class_get_namespace, "il2cpp_class_get_namespace");
        ok &= _CheckPtr(il2cpp_class_get_name, "il2cpp_class_get_name");
        ok &= _CheckPtr(il2cpp_class_get_fields, "il2cpp_class_get_fields");
        ok &= _CheckPtr(il2cpp_class_get_methods, "il2cpp_class_get_methods");
        ok &= _CheckPtr(il2cpp_field_get_name, "il2cpp_field_get_name");
        ok &= _CheckPtr(il2cpp_method_get_name, "il2cpp_method_get_name");
        ok &= _CheckPtr(il2cpp_get_corlib, "il2cpp_get_corlib");

        if (!ok) {
            Log::Debug("init: not all needed functions initialized!");
        } else {
            Log::Debug("init: initialized needed functions");
        }

        return ok;
    }
}