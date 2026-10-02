#pragma once

#include <string>
#include <vector>
#include "functions.h"

namespace Api {
    struct Class {
    private:
        template <typename T, typename Fn>
        std::vector<T*> DoIterator(const Fn& f) {
            std::vector<T*> vec;

            void* iter = nullptr;
            while (T* v = f(this, &iter)) {
                if (v) vec.push_back(v);
            }

            return vec;
        }

    public:
        Class() = delete;

        std::string GetNamespace() {
            auto s = Functions::il2cpp_class_get_namespace(this);
            return {s ? s : ""};
        }

        std::string GetName() {
            auto s = Functions::il2cpp_class_get_name(this);
            return {s ? s : ""};
        }

        std::vector<Method*> GetMethods() {
            return DoIterator<Method>(Functions::il2cpp_class_get_methods);
        }

        std::vector<Field*> GetFields() {
            return DoIterator<Field>(Functions::il2cpp_class_get_fields);
        }

        std::vector<Property*> GetProperties() {
            return DoIterator<Property>(Functions::il2cpp_class_get_properties);
        }

        std::vector<Class*> GetInterfaces() {
            return DoIterator<Class>(Functions::il2cpp_class_get_interfaces);
        }

        Class* GetParent() {
            return Functions::il2cpp_class_get_parent(this);
        }

        static Class* FromType(Type* typ) {
            return Functions::il2cpp_class_from_type(typ);
        }

        static Class* FromSystemType(Object* typ) {
            return Functions::il2cpp_class_from_system_type(typ);
        }

        bool IsGeneric() {
            return Functions::il2cpp_class_is_generic(this);
        }

        bool IsInflated() {
            return Functions::il2cpp_class_is_inflated(this);
        }

        Class* GetElementClass() {
            return Functions::il2cpp_class_get_element_class(this);
        }

        Type* GetType() {
            return Functions::il2cpp_class_get_type(this);
        }

        Class* GetDeclaringClass() {
            return Functions::il2cpp_class_get_declaring_type(this);
        }

        int GetFlags() {
            return Functions::il2cpp_class_get_flags(this);
        }

        bool IsEnum() {
            return Functions::il2cpp_class_is_enum(this);
        }

        bool IsValueType() {
            return Functions::il2cpp_class_is_valuetype(this);
        }

        Type* GetEnumBaseType() {
            return Functions::il2cpp_class_enum_basetype(this);
        }

        int32_t GetInstanceSize() {
            return Functions::il2cpp_class_instance_size(this);
        }

        bool IsAssignableFrom(Class* ext) {
            return Functions::il2cpp_class_is_assignable_from(this, ext);
        }
    };
}