#pragma once

#include <string>
#include <vector>
#include "functions.h"
#include "field.h"
#include "method.h"

namespace Dumper::Api {
    struct Class {
        std::string GetNamespace() {
            auto s = Functions::il2cpp_class_get_namespace(this);
            return {s ? s : ""};
        }

        std::string GetName() {
            auto s = Functions::il2cpp_class_get_name(this);
            return {s ? s : ""};
        }

        std::vector<Method*> GetMethods() {
            std::vector<Method*> vec;

            void* iter = nullptr;
            while (void* met = Functions::il2cpp_class_get_methods(this, &iter)) {
                vec.push_back((Method*) met);
            }

            return vec;
        }

        std::vector<Field*> GetFields() {
            std::vector<Field*> vec;

            void* iter = nullptr;
            while (void* met = Functions::il2cpp_class_get_fields(this, &iter)) {
                vec.push_back((Field*) met);
            }

            return vec;
        }
    };
}