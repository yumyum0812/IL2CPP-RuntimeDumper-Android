#pragma once

#include "functions.h"
#include <string>

namespace Dumper::Api {
    struct Field {
        std::string GetName() {
            auto s = Functions::il2cpp_field_get_name(this);
            return {s ? s : ""};
        }
    };
}