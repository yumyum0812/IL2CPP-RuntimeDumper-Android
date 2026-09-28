#pragma once

#include "functions.h"

namespace Dumper::Api {
    struct Array {
        int GetLength() {
            return Functions::il2cpp_array_length(this);
        }
    };
}