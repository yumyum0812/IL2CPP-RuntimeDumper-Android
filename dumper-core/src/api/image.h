#pragma once

#include "functions.h"
#include <string>
#include <vector>

namespace Api {
    struct Image {
        Image() = delete;
        
        static Image* GetCorlib() {
            return Functions::get_corlib();
        }
        
        std::string GetName() {
            auto s = Functions::image_get_name(this);
            return {s ? s : ""};
        }

        size_t GetClassCount() {
            return Functions::image_get_class_count(this);
        }

        std::vector<Class*> GetClasses() {
            size_t count = GetClassCount();

            std::vector<Class*> ret;
            ret.reserve(count);
            for (int i = 0; i < count; i++) {
                Class* c = Functions::image_get_class(this, i);
                if (c) ret.push_back(c);
            }

            return ret;
        }

        Class* FindClass(const std::string& ns, const std::string& nm) {
            return Functions::class_from_name(this, ns.c_str(), nm.c_str());
        }
    };
}