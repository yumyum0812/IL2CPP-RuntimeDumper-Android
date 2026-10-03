#pragma once
#include "functions.h"
#include <string>

namespace Api {
    struct String {
        String() = delete;
        
        static String* New(const char* cstr) {
            return Functions::string_new(cstr);
        }

        std::u16string GetChars() {
            auto s = Functions::string_chars(this);
            if (!s) return {};
            
            auto len = static_cast<size_t>(Functions::string_length(this));
            return {s, len};
        }
    };
}