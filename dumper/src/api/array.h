#pragma once

#include "functions.h"

namespace Api {
    struct Array {
        Array() = delete;
        
        inline int GetLength() {
            return Functions::array_length(this);
        }
    };
}