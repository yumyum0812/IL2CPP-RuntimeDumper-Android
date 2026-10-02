#pragma once

#include "functions.h"

namespace Api {
    struct Object {
        Object() = delete;
        
        Class* GetClass() {
            return Functions::il2cpp_object_get_class(this);
        }

        void* Unbox() {
            return Functions::il2cpp_object_unbox(this);
        }

        Method* GetVirtualMethod(Method* base) {
            return Functions::il2cpp_object_get_virtual_method(this, base);
        }
    };
}