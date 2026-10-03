#pragma once

#include "functions.h"

namespace Api {
    struct Object {
        Object() = delete;
        
        Class* GetClass() {
            return Functions::object_get_class(this);
        }

        void* Unbox() {
            return Functions::object_unbox(this);
        }

        Method* GetVirtualMethod(Method* base) {
            return Functions::object_get_virtual_method(this, base);
        }
    };
}