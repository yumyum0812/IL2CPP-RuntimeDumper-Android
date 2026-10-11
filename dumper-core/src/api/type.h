#pragma once

#include <string>
#include "functions.h"

namespace Api {
    struct Type {
        Type() = delete;

        std::string GetName() {
            auto s = Functions::type_get_name(this);
            return {s ? s : ""};
        }

        int GetTypeEnum() {
            return Functions::type_get_type(this);
        }

        int GetAttrs() {
            return Functions::type_get_attrs(this);
        }

        int IsByRef() {
            return Functions::type_is_byref(this);
        }

        Object* GetObject() {
            return Functions::type_get_object(this);
        }
    };
}