#pragma once

#include "functions.h"

namespace Api {
    struct Array {
        Array() = delete;
        
        int GetLength() {
            return Functions::il2cpp_array_length(this);
        }
    };
}