#pragma once

#include <cstddef>
#include <vector>
#include <string>
#include <format>
#include <xdl.h>

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
}

namespace Api::Functions {
    struct IApiCall {
        virtual ~IApiCall() = default;

        virtual bool Valid() const = 0;
        virtual std::string Name() const = 0;
        virtual void Assign(void* ptr) = 0;
        virtual void* GetAddr() = 0;
    };

    inline std::vector<IApiCall*> _apiCalls{};

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
            if (!fn)
                throw std::runtime_error(std::format("call to null function: {}", _name));
            return fn(std::forward<A>(a)...);
        }

        void* GetAddr() override {
            return (void*) fn;
        }
    };

#define _API_FUNC(sym, F) auto sym = _ApiCall<F>(#sym)

    _API_FUNC(il2cpp_get_corlib, Image*());

    _API_FUNC(il2cpp_domain_get, Domain*());
    _API_FUNC(il2cpp_domain_get_assemblies, Assembly**(Domain* domain, size_t* szOut));

    _API_FUNC(il2cpp_assembly_get_image, Image*(Assembly* assembly));

    _API_FUNC(il2cpp_thread_attach, Thread*(Domain* domain));
    _API_FUNC(il2cpp_thread_detach, void(Thread* thread));

    _API_FUNC(il2cpp_image_get_name, const char*(Image* image));
    _API_FUNC(il2cpp_image_get_class_count, size_t(Image* image));
    _API_FUNC(il2cpp_image_get_class, Class*(Image* image, size_t index));

    _API_FUNC(il2cpp_class_get_namespace, const char*(Class* klass));
    _API_FUNC(il2cpp_class_get_name, const char*(Class* klass));
    _API_FUNC(il2cpp_class_get_fields, Field*(Class* klass, void** iter));
    _API_FUNC(il2cpp_class_get_methods, Method*(Class* klass, void** iter));
    _API_FUNC(il2cpp_class_get_properties, Property*(Class* klass, void** iter));
    _API_FUNC(il2cpp_class_get_interfaces, Class*(Class* klass, void** iter));
    _API_FUNC(il2cpp_class_get_parent, Class*(Class* klass));
    _API_FUNC(il2cpp_class_from_type, Class*(Type* typ));
    _API_FUNC(il2cpp_class_from_name, Class*(Image* img, const char* ns, const char* nm));
    _API_FUNC(il2cpp_class_from_system_type, Class*(Object* obj));
    _API_FUNC(il2cpp_class_get_type, Type*(Class* klass));
    _API_FUNC(il2cpp_class_is_generic, bool(Class* klass));
    _API_FUNC(il2cpp_class_is_inflated, bool(Class* klass));
    _API_FUNC(il2cpp_class_get_element_class, Class*(Class* klass));
    _API_FUNC(il2cpp_class_get_declaring_type, Class*(Class* klass));
    _API_FUNC(il2cpp_class_get_flags, int(Class* klass));
    _API_FUNC(il2cpp_class_is_valuetype, bool(Class* klass));
    _API_FUNC(il2cpp_class_is_enum, bool(Class* klass));
    _API_FUNC(il2cpp_class_enum_basetype, Type*(Class* klass));
    _API_FUNC(il2cpp_class_instance_size, uintptr_t(Class* klass));
    _API_FUNC(il2cpp_class_is_assignable_from, bool(Class* base, Class* ext));

    _API_FUNC(il2cpp_field_get_name, const char*(Field* field));
    _API_FUNC(il2cpp_field_get_type, Type*(Field* field));
    _API_FUNC(il2cpp_field_get_offset, size_t(Field* field));
    _API_FUNC(il2cpp_field_get_flags, int(Field* field));
    _API_FUNC(il2cpp_field_static_get_value, void(Field* field, void* value));

    _API_FUNC(il2cpp_method_get_name, const char*(Method* method));
    _API_FUNC(il2cpp_method_get_class, Class*(Method* method));
    _API_FUNC(il2cpp_method_get_return_type, Type*(Method* method));
    _API_FUNC(il2cpp_method_get_param, Type*(Method* method, size_t index));
    _API_FUNC(il2cpp_method_get_param_count, size_t(Method* method));
    _API_FUNC(il2cpp_method_get_param_name, const char*(Method* method, size_t index));
    _API_FUNC(il2cpp_method_get_flags, int(Method* method, uint32_t* iflags));
    _API_FUNC(il2cpp_method_is_generic, bool(Method* method));
    _API_FUNC(il2cpp_method_is_inflated, bool(Method* method));
    _API_FUNC(il2cpp_method_get_object, Object*(Method* method));

    _API_FUNC(il2cpp_type_get_name, const char*(Type* typ));
    _API_FUNC(il2cpp_type_get_type, int(Type* typ));
    _API_FUNC(il2cpp_type_get_attrs, int(Type* typ));
    _API_FUNC(il2cpp_type_get_object, Object*(Type* typ));
    _API_FUNC(il2cpp_type_is_byref, bool(Type* typ));

    _API_FUNC(il2cpp_property_get_get_method, Method*(Property* prop));
    _API_FUNC(il2cpp_property_get_set_method, Method*(Property* prop));
    _API_FUNC(il2cpp_property_get_name, const char*(Property* prop));

    _API_FUNC(il2cpp_object_get_class, Class*(Object* obj));
    _API_FUNC(il2cpp_object_unbox, void*(Object* obj));
    _API_FUNC(il2cpp_object_get_virtual_method, Method*(Object* obj, Method* base));

    _API_FUNC(il2cpp_string_new, String*(const char* cstr));
    _API_FUNC(il2cpp_string_chars, const char16_t*(String* s));
    _API_FUNC(il2cpp_string_length, int(String* s));

    _API_FUNC(il2cpp_array_length, int(Array* typ));

    _API_FUNC(il2cpp_runtime_invoke, Object*(Method* met, void* obj, void** args, Object** exc));
}