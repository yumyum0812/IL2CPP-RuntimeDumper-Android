#pragma once

#include <string>
#include "functions.h"

namespace Api {
    struct Type {
        Type() = delete;

        std::string GetName() {
            auto s = Functions::il2cpp_type_get_name(this);
            return {s ? s : ""};
        }

        int GetTypeEnum() {
            return Functions::il2cpp_type_get_type(this);
        }

        int GetAttrs() {
            return Functions::il2cpp_type_get_attrs(this);
        }

        int IsByRef() {
            return Functions::il2cpp_type_is_byref(this);
        }

        Object* GetObject() {
            return Functions::il2cpp_type_get_object(this);
        }
    };
}