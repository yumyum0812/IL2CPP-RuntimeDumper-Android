#pragma once

#include "functions.h"
#include "image.h"

namespace Dumper::Api {
    struct Assembly {
        Image* GetImage() {
            return Functions::il2cpp_assembly_get_image(this);
        }
    };
}