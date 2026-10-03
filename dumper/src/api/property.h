#pragma once

#include <string>
#include "functions.h"

namespace Api {
    struct Property {
        Property() = delete;
        
        std::string GetName() {
            auto s = Functions::property_get_name(this);
            return {s ? s : ""};
        }
        Method* GetGetMethod() {
            return Functions::property_get_get_method(this);
        }

        Method* GetSetMethod() {
            return Functions::property_get_set_method(this);
        }
    };
}