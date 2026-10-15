#pragma once
#include "functions.h"
#include <vector>

namespace Api {
    struct String {
        String() = delete;
        
        static String* New(const char* cstr) {
            return Functions::string_new(cstr);
        }

        std::vector<char16_t> GetChars() {
            auto s = Functions::string_chars(this);
            if (!s) return {};
            
            auto len = static_cast<size_t>(Functions::string_length(this));
            return std::vector<char16_t>(s, s + len);
        }
    };
}