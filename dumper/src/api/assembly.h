#pragma once

#include "functions.h"

namespace Api {
    struct Assembly {
        Assembly() = delete;
        
        Image* GetImage() {
            return Functions::il2cpp_assembly_get_image(this);
        }
    };
}