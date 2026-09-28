#pragma once

#include <string>
#include "functions.h"
#include "type.h"

namespace Dumper::Api {
    struct Method {
        void* methodPtr;

        std::string GetName() {
            auto s = Functions::il2cpp_method_get_name(this);
            return {s ? s : ""};
        }

        Type* GetReturnType() {
            return Functions::il2cpp_method_get_return_type(this);
        }

        size_t GetParamCount() {
            return Functions::il2cpp_method_get_param_count(this);
        }
        
        std::string GetParamName(size_t index) {
            auto s = Functions::il2cpp_method_get_param_name(this, index);
            return {s ? s : ""};
        }

        Type* GetParam(size_t index) {
            return Functions::il2cpp_method_get_param(this, index);
        }

        void* GetPointer() {
            return methodPtr;
        }

        bool IsGeneric() {
            return Functions::il2cpp_method_is_generic(this);
        }

        bool IsInflated() {
            return Functions::il2cpp_method_is_inflated(this);
        }

        template<typename R, typename... A>
        R Invoke(A... args) {
            return ((R(*)(A...))methodPtr)(std::forward<A>(args)...);
        }

        int GetFlags() {
            return Functions::il2cpp_method_get_flags(this, nullptr);
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

            void* exc = nullptr;
            return Functions::il2cpp_runtime_invoke(this, obj, paramArray, &exc);
        }

        Object* GetObject() {
            return Functions::il2cpp_method_get_object(this);
        }
    };
}