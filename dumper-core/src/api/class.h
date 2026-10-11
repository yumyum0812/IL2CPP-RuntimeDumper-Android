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
            auto s = Functions::class_get_namespace(this);
            return {s ? s : ""};
        }

        std::string GetName() {
            auto s = Functions::class_get_name(this);
            return {s ? s : ""};
        }

        std::vector<Method*> GetMethods() {
            return DoIterator<Method>(Functions::class_get_methods);
        }

        std::vector<Field*> GetFields() {
            return DoIterator<Field>(Functions::class_get_fields);
        }

        std::vector<Property*> GetProperties() {
            return DoIterator<Property>(Functions::class_get_properties);
        }

        std::vector<Class*> GetInterfaces() {
            return DoIterator<Class>(Functions::class_get_interfaces);
        }

        Class* GetParent() {
            return Functions::class_get_parent(this);
        }

        static Class* FromType(Type* typ) {
            return Functions::class_from_type(typ);
        }

        static Class* FromSystemType(Object* typ) {
            return Functions::class_from_system_type(typ);
        }

        bool IsGeneric() {
            return Functions::class_is_generic(this);
        }

        bool IsInflated() {
            return Functions::class_is_inflated(this);
        }

        Class* GetElementClass() {
            return Functions::class_get_element_class(this);
        }

        Type* GetType() {
            return Functions::class_get_type(this);
        }

        Class* GetDeclaringClass() {
            return Functions::class_get_declaring_type(this);
        }

        int GetFlags() {
            return Functions::class_get_flags(this);
        }

        bool IsEnum() {
            return Functions::class_is_enum(this);
        }

        bool IsValueType() {
            return Functions::class_is_valuetype(this);
        }

        Type* GetEnumBaseType() {
            return Functions::class_enum_basetype(this);
        }

        int32_t GetInstanceSize() {
            return Functions::class_instance_size(this);
        }

        bool IsAssignableFrom(Class* ext) {
            return Functions::class_is_assignable_from(this, ext);
        }
    };
}