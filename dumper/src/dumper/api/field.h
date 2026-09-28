#pragma once

#include <string>
#include "functions.h"
#include "type.h"

namespace Dumper::Api {
    struct Field {
        std::string GetName() {
            auto s = Functions::il2cpp_field_get_name(this);
            return {s ? s : ""};
        }

        Type* GetType() {
            return Functions::il2cpp_field_get_type(this);
        }

        size_t GetOffset() {
            return Functions::il2cpp_field_get_offset(this);
        }

        int GetFlags() {
            return Functions::il2cpp_field_get_flags(this);
        }

        template<typename T>
        T GetStaticValue() {
            T value;
            Functions::il2cpp_field_static_get_value(this, &value);
            return value;
        }
    };
}