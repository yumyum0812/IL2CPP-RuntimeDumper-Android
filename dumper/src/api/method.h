#pragma once

#include <string>
#include "functions.h"

namespace Api {
    struct Method {
        void* methodPtr;

        Method() = delete;

        std::string GetName() {
            auto s = Functions::method_get_name(this);
            return {s ? s : ""};
        }

        Type* GetReturnType() {
            return Functions::method_get_return_type(this);
        }

        size_t GetParamCount() {
            return Functions::method_get_param_count(this);
        }
        
        std::string GetParamName(size_t index) {
            auto s = Functions::method_get_param_name(this, index);
            return {s ? s : ""};
        }

        Type* GetParam(size_t index) {
            return Functions::method_get_param(this, index);
        }

        void* GetPointer() {
            return methodPtr;
        }

        bool IsGeneric() {
            return Functions::method_is_generic(this);
        }

        bool IsInflated() {
            return Functions::method_is_inflated(this);
        }

        int GetFlags() {
            return Functions::method_get_flags(this, nullptr);
        }

        template<typename... A>
        Object* Invoke(void* obj, A&... args) {
            constexpr std::size_t argCount = sizeof...(A);
            void* paramArray[argCount > 0 ? argCount : 1] = { nullptr };

            if constexpr (argCount > 0) {
                std::size_t index = 0;
                ([&]() {
                    if constexpr (std::is_pointer_v<std::decay_t<A>>) {
                        paramArray[index++] = const_cast<void*>(static_cast<const void*>(args));
                    } else {
                        paramArray[index++] = const_cast<void*>(static_cast<const void*>(&args));
                    }
                }(), ...);
            }

            Object* exc = nullptr;
            return Functions::runtime_invoke(this, obj, paramArray, &exc);
        }

        template <typename T, typename... A>
        T InvokeDirect(A... args) {
            return reinterpret_cast<T(*)(A..., Method*)>(methodPtr)(std::forward<A>(args)..., nullptr);
        }

        Object* GetObject() {
            return Functions::method_get_object(this);
        }

        Class* GetClass() {
            return Functions::method_get_class(this);
        }
    };
}