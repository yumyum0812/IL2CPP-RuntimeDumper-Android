#pragma once

#include <cstddef>
#include <vector>
#include <string>
#include <xdl.h>
#include <android/log.h>
#include "../dumper_log.h"

namespace Api {
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

    class Functions;

    struct IApiCall {
        virtual ~IApiCall() = default;

        virtual bool Valid() const = 0;
        virtual std::string Name() const = 0;
        virtual void Assign(void* ptr) = 0;
        virtual void* GetPointer() = 0;
    };
}

class Api::Functions {
    inline static std::vector<IApiCall*> _apiCalls{};

    template <typename R, typename... A>
    struct _ApiCall;

    template <typename R, typename... A>
    class _ApiCall<R(A...)> : public IApiCall {
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
            if (!fn) {
                DumperLog::Assert("fn == nullptr", "call to uninitialize api call: {}", _name);
            }

            // DumperLog::Debug("calling: {}", _name);
            // if constexpr (not std::is_void_v<R>) {
            //     R r = fn(std::forward<A>(a)...);
            //     DumperLog::Debug("call ok: {}", _name);
            //     return r;
            // } else {
            //     fn(std::forward<A>(a)...);
            //     DumperLog::Debug("call ok: {}", _name);
            // }
            return fn(std::forward<A>(a)...);
        }

        void* GetPointer() override {
            return reinterpret_cast<void*>(fn);
        }
    };

    Functions() = delete;

#define _API_FUNC(sym, F) inline static auto sym = _ApiCall<F>("il2cpp_" #sym)

public:

    _API_FUNC(get_corlib, Image*());

    _API_FUNC(domain_get, Domain*());
    _API_FUNC(domain_get_assemblies, Assembly**(Domain* domain, size_t* szOut));

    _API_FUNC(assembly_get_image, Image*(Assembly* assembly));

    _API_FUNC(thread_current, Thread*());
    _API_FUNC(thread_attach, Thread*(Domain* domain));
    _API_FUNC(thread_detach, void(Thread* thread));

    _API_FUNC(image_get_name, const char*(Image* image));
    _API_FUNC(image_get_class_count, size_t(Image* image));
    _API_FUNC(image_get_class, Class*(Image* image, size_t index));

    _API_FUNC(class_get_namespace, const char*(Class* klass));
    _API_FUNC(class_get_name, const char*(Class* klass));
    _API_FUNC(class_get_fields, Field*(Class* klass, void** iter));
    _API_FUNC(class_get_methods, Method*(Class* klass, void** iter));
    _API_FUNC(class_get_properties, Property*(Class* klass, void** iter));
    _API_FUNC(class_get_interfaces, Class*(Class* klass, void** iter));
    _API_FUNC(class_get_parent, Class*(Class* klass));
    _API_FUNC(class_from_type, Class*(Type* typ));
    _API_FUNC(class_from_name, Class*(Image* img, const char* ns, const char* nm));
    _API_FUNC(class_from_system_type, Class*(Object* obj));
    _API_FUNC(class_get_type, Type*(Class* klass));
    _API_FUNC(class_is_generic, bool(Class* klass));
    _API_FUNC(class_is_inflated, bool(Class* klass));
    _API_FUNC(class_get_element_class, Class*(Class* klass));
    _API_FUNC(class_get_declaring_type, Class*(Class* klass));
    _API_FUNC(class_get_flags, int(Class* klass));
    _API_FUNC(class_is_valuetype, bool(Class* klass));
    _API_FUNC(class_is_enum, bool(Class* klass));
    _API_FUNC(class_enum_basetype, Type*(Class* klass));
    _API_FUNC(class_instance_size, uintptr_t(Class* klass));
    _API_FUNC(class_is_assignable_from, bool(Class* base, Class* ext));

    _API_FUNC(field_get_name, const char*(Field* field));
    _API_FUNC(field_get_type, Type*(Field* field));
    _API_FUNC(field_get_offset, size_t(Field* field));
    _API_FUNC(field_get_flags, int(Field* field));
    _API_FUNC(field_static_get_value, void(Field* field, void* value));

    _API_FUNC(method_get_name, const char*(Method* method));
    _API_FUNC(method_get_class, Class*(Method* method));
    _API_FUNC(method_get_return_type, Type*(Method* method));
    _API_FUNC(method_get_param, Type*(Method* method, size_t index));
    _API_FUNC(method_get_param_count, size_t(Method* method));
    _API_FUNC(method_get_param_name, const char*(Method* method, size_t index));
    _API_FUNC(method_get_flags, int(Method* method, uint32_t* iflags));
    _API_FUNC(method_is_generic, bool(Method* method));
    _API_FUNC(method_is_inflated, bool(Method* method));
    _API_FUNC(method_get_object, Object*(Method* method, Class* refClass));

    _API_FUNC(type_get_name, const char*(Type* typ));
    _API_FUNC(type_get_type, int(Type* typ));
    _API_FUNC(type_get_attrs, int(Type* typ));
    _API_FUNC(type_get_object, Object*(Type* typ));
    _API_FUNC(type_is_byref, bool(Type* typ));

    _API_FUNC(property_get_get_method, Method*(Property* prop));
    _API_FUNC(property_get_set_method, Method*(Property* prop));
    _API_FUNC(property_get_name, const char*(Property* prop));

    _API_FUNC(object_get_class, Class*(Object* obj));
    _API_FUNC(object_unbox, void*(Object* obj));
    _API_FUNC(object_get_virtual_method, Method*(Object* obj, Method* base));

    _API_FUNC(string_new, String*(const char* cstr));
    _API_FUNC(string_chars, const char16_t*(String* s));
    _API_FUNC(string_length, int(String* s));

    _API_FUNC(array_length, int(Array* typ));

    _API_FUNC(runtime_invoke, Object*(Method* met, void* obj, void** args, Object** exc));

    static std::vector<IApiCall*> GetCalls() {
        return _apiCalls;
    }
};