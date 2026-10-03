#pragma once

#include <string>
#include "functions.h"
#include "type.h"

namespace Api {
    struct Field {
        Field() = delete;
        
        std::string GetName() {
            auto s = Functions::field_get_name(this);
            return {s ? s : ""};
        }

        Type* GetType() {
            return Functions::field_get_type(this);
        }

        size_t GetOffset() {
            return Functions::field_get_offset(this);
        }

        int GetFlags() {
            return Functions::field_get_flags(this);
        }

        template<typename T>
        T GetStaticValue() {
            T value;
            Functions::field_static_get_value(this, &value);
            return value;
        }
    };
}