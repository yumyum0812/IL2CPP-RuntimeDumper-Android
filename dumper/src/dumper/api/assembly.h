#pragma once

#include "functions.h"
#include "image.h"

namespace Dumper::Api {
    struct Assembly {
        Image* GetImage() {
            return (Image*) Functions::il2cpp_assembly_get_image(this);
        }
    };
}