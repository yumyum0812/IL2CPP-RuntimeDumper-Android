#pragma once

#include "functions.h"

namespace Api {
    struct Assembly {
        Assembly() = delete;
        
        Image* GetImage() {
            return Functions::assembly_get_image(this);
        }
    };
}