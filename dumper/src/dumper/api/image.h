#pragma once

#include "functions.h"
#include "class.h"
#include <string>
#include <vector>

namespace Dumper::Api {
    struct Image {
        std::string GetName() {
            auto s = Functions::il2cpp_image_get_name(this);
            return {s ? s : ""};
        }

        size_t GetClassCount() {
            return Functions::il2cpp_image_get_class_count(this);
        }

        std::vector<Class*> GetClasses() {
            size_t count = GetClassCount();

            std::vector<Class*> ret;
            ret.reserve(count);
            for (int i = 0; i < count; i++) {
                Class* c = Functions::il2cpp_image_get_class(this, i);
                ret.push_back(c);
            }

            return ret;
        }

        Class* FindClass(const std::string& ns, const std::string& nm) {
            return Functions::il2cpp_class_from_name(this, ns.c_str(), nm.c_str());
        }
    };
}