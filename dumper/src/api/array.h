#pragma once

#include "functions.h"

namespace Api {
    struct Array {
        Array() = delete;
        
        int GetLength() {
            return Functions::array_length(this);
        }
    };
}