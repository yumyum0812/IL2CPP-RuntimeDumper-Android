#pragma once

#include <string>
#include "functions.h"
#include <filesystem>

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

            Object* exc = nullptr;
            auto ret = Functions::il2cpp_runtime_invoke(this, obj, paramArray, &exc);
            if (exc) {
                auto k = Functions::il2cpp_object_get_class(exc);
                auto aa = Functions::il2cpp_string_chars(*(String**)(uintptr_t(exc) + 0x18));
                auto aaa = std::filesystem::path(aa).string();

                Log::Debug("e: {}", Functions::il2cpp_class_get_name(k));
                Log::Debug("  msg: {}", aaa);

                Log::Debug("m: {}", this->GetName());
                Log::Debug(" k: {}", Functions::il2cpp_class_get_name(this->GetClass()));
                Log::Debug("obj: {}", Functions::il2cpp_class_get_name(Functions::il2cpp_object_get_class((Object*)obj)));
            }
            return ret;
        }

        template <typename T, typename... A>
        T InvokeDirect(A... args) {
            return reinterpret_cast<T(*)(A..., Method*)>(methodPtr)(std::forward<A>(args)..., nullptr);
        }

        Object* GetObject() {
            return Functions::il2cpp_method_get_object(this);
        }

        Class* GetClass() {
            return Functions::il2cpp_method_get_class(this);
        }
    };
}