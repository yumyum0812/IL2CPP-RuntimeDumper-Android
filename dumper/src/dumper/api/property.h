#pragma once

#include <string>
#include "functions.h"

namespace Dumper::Api {
    struct Property {
        std::string GetName() {
            auto s = Functions::il2cpp_property_get_name(this);
            return {s ? s : ""};
        }
        Method* GetGetMethod() {
            return Functions::il2cpp_property_get_get_method(this);
        }

        Method* GetSetMethod() {
            return Functions::il2cpp_property_get_set_method(this);
        }
    };
}