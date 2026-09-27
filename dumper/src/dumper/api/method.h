#pragma once

#include "functions.h"
#include <string>

namespace Dumper::Api {
    struct Method {
        std::string GetName() {
            auto s = Functions::il2cpp_method_get_name(this);
            return {s ? s : ""};
        }
    };
}