#pragma once
#include "functions.h"
#include <string>

namespace Dumper::Api {
    struct String {
        std::u16string GetChars() {
            auto s = Functions::il2cpp_string_chars(this);
            if (!s) return {};
            
            auto len = static_cast<size_t>(Functions::il2cpp_string_length(this));
            return {s, len};
        }
    };
}