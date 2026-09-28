#pragma once

#include <chrono>
#include <cstddef>
#include <cstdint>
#include <thread>
#include <xdl.h>
#include "../log.h"

namespace Dumper::Api {
    struct Domain;
    struct Thread;
    struct Assembly;
    struct Image;
    struct Type;
    struct Field;
    struct Method;
    struct Property;
    struct Class;
    struct Object;
    struct Array;
    struct String;
}

namespace Dumper::Api::Functions {
    struct _IApiCall {
        virtual ~_IApiCall() = default;

        virtual bool Valid() const = 0;
        virtual std::string Name() const = 0;
        virtual void Assign(void* ptr) = 0;
        virtual void* GetAddr() = 0;
    };

    inline std::vector<_IApiCall*> _apiCalls{};

    template <typename R, typename... A>
    struct _ApiCall;

    template <typename R, typename... A>
    class _ApiCall<R(A...)> : public _IApiCall {
        std::string _name;
        R (* fn)(A...) = nullptr;
        
    public:
        _ApiCall(std::string_view name): _name(name) {
            _apiCalls.push_back(this);
        }

        bool Valid() const override {
            return fn != nullptr;
        }

        std::string Name() const override {
            return std::string(_name);
        }

        void Assign(void* ptr) override {
            fn = reinterpret_cast<R(*)(A...)>(ptr);
        }

        R operator() (A... a) const {
            return fn(std::forward<A>(a)...);
        }

        void* GetAddr() override {
            return (void*) fn;
        }
    };

    #define DUMPER_API_DEF(sym, F) auto sym = _ApiCall<F>(#sym)


    DUMPER_API_DEF(il2cpp_domain_get, Domain*());
    DUMPER_API_DEF(il2cpp_domain_get_assemblies, Assembly**(Domain* domain, size_t* szOut));

    DUMPER_API_DEF(il2cpp_assembly_get_image, Image*(Assembly* assembly));

    DUMPER_API_DEF(il2cpp_thread_attach, Thread*(Domain* domain));
    DUMPER_API_DEF(il2cpp_thread_detach, void(Thread* thread));

    DUMPER_API_DEF(il2cpp_image_get_name, const char*(Image* image));
    DUMPER_API_DEF(il2cpp_image_get_class_count, size_t(Image* image));
    DUMPER_API_DEF(il2cpp_image_get_class, Class*(Image* image, size_t index));

    DUMPER_API_DEF(il2cpp_class_get_namespace, const char*(Class* klass));
    DUMPER_API_DEF(il2cpp_class_get_name, const char*(Class* klass));
    DUMPER_API_DEF(il2cpp_class_get_fields, Field*(Class* klass, void** iter));
    DUMPER_API_DEF(il2cpp_class_get_methods, Method*(Class* klass, void** iter));
    DUMPER_API_DEF(il2cpp_class_get_properties, Property*(Class* klass, void** iter));
    DUMPER_API_DEF(il2cpp_class_get_interfaces, Class*(Class* klass, void** iter));
    DUMPER_API_DEF(il2cpp_class_get_parent, Class*(Class* klass));
    DUMPER_API_DEF(il2cpp_class_from_type, Class*(Type* typ));
    DUMPER_API_DEF(il2cpp_class_from_name, Class*(Image* img, const char* ns, const char* nm));
    DUMPER_API_DEF(il2cpp_class_from_system_type, Class*(Object* obj));
    DUMPER_API_DEF(il2cpp_class_get_type, Type*(Class* klass));
    DUMPER_API_DEF(il2cpp_class_is_generic, bool(Class* klass));
    DUMPER_API_DEF(il2cpp_class_is_inflated, bool(Class* klass));
    DUMPER_API_DEF(il2cpp_class_get_element_class, Class*(Class* klass));
    DUMPER_API_DEF(il2cpp_class_get_declaring_type, Class*(Class* klass));
    DUMPER_API_DEF(il2cpp_class_get_flags, int(Class* klass));
    DUMPER_API_DEF(il2cpp_class_is_valuetype, bool(Class* klass));
    DUMPER_API_DEF(il2cpp_class_is_enum, bool(Class* klass));
    DUMPER_API_DEF(il2cpp_class_enum_basetype, Type*(Class* klass));
    DUMPER_API_DEF(il2cpp_class_instance_size, uintptr_t(Class* klass));

    DUMPER_API_DEF(il2cpp_field_get_name, const char*(Field* field));
    DUMPER_API_DEF(il2cpp_field_get_type, Type*(Field* field));
    DUMPER_API_DEF(il2cpp_field_get_offset, size_t(Field* field));
    DUMPER_API_DEF(il2cpp_field_get_flags, int(Field* field));
    DUMPER_API_DEF(il2cpp_field_static_get_value, void(Field* field, void* value));

    DUMPER_API_DEF(il2cpp_method_get_name, const char*(Method* method));
    DUMPER_API_DEF(il2cpp_method_get_return_type, Type*(Method* method));
    DUMPER_API_DEF(il2cpp_method_get_param, Type*(Method* method, size_t index));
    DUMPER_API_DEF(il2cpp_method_get_param_count, size_t(Method* method));
    DUMPER_API_DEF(il2cpp_method_get_param_name, const char*(Method* method, size_t index));
    DUMPER_API_DEF(il2cpp_method_get_flags, int(Method* method, uint32_t* iflags));
    DUMPER_API_DEF(il2cpp_method_is_generic, bool(Method* method));
    DUMPER_API_DEF(il2cpp_method_is_inflated, bool(Method* method));
    DUMPER_API_DEF(il2cpp_method_get_object, Object*(Method* method));

    DUMPER_API_DEF(il2cpp_type_get_name, const char*(Type* typ));
    DUMPER_API_DEF(il2cpp_type_get_type, int(Type* typ));
    DUMPER_API_DEF(il2cpp_type_get_attrs, int(Type* typ));
    DUMPER_API_DEF(il2cpp_type_get_object, Object*(Type* typ));
    DUMPER_API_DEF(il2cpp_type_is_byref, bool(Type* typ));

    DUMPER_API_DEF(il2cpp_property_get_get_method, Method*(Property* prop));
    DUMPER_API_DEF(il2cpp_property_get_set_method, Method*(Property* prop));
    DUMPER_API_DEF(il2cpp_property_get_name, const char*(Property* prop));

    DUMPER_API_DEF(il2cpp_string_chars, const char16_t*(String* s));
    DUMPER_API_DEF(il2cpp_string_length, int(String* s));

    DUMPER_API_DEF(il2cpp_array_length, int(Array* typ));
    DUMPER_API_DEF(il2cpp_get_corlib, Image*());
    DUMPER_API_DEF(il2cpp_runtime_invoke, Object*(Method* met, void* obj, void** args, void** exc));


    class LibraryResolver {
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
            base = (uintptr_t) info.dli_fbase;

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

        uintptr_t RVAToOffset(uintptr_t rva) {
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
            return xdl_sym(handle, name.c_str(), nullptr);
        }
    };


    class ApiResolver {
    public:
        ApiResolver() = default;
        virtual void Resolve(LibraryResolver& lf) = 0;
    };


    class SymApiResolver : public ApiResolver {
    public:
        void Resolve(LibraryResolver& lf) override {
            for (auto f : _apiCalls) {
                auto name = f->Name();
                void* ptr = lf.ResolveSymbol(name);
                if (!ptr) {
                    Log::Debug("apiresolver: symbol not found: {}", name);
                    continue;
                }
                f->Assign(ptr);
            }
        }
    };

    
    bool InitApi(LibraryResolver& libRes, ApiResolver& res) {
        Log::Debug("init: resolving library...");
        libRes.Resolve();
        
        Log::Debug("init: resolving api...");
        res.Resolve(libRes);

        bool ok = true;
        for (auto f : _apiCalls) {
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
}