#pragma once
#include "functions.h"

namespace Api {
    struct Domain {
        Domain() = delete;
        
        static Domain* GetInstance() {
            return Functions::domain_get();
        }

        std::vector<Assembly*> GetAssemblies() {
            size_t size = 0;
            auto asms = Functions::domain_get_assemblies(this, &size);

            std::vector<Assembly*> vec;
            vec.reserve(size);
            
            for (int i = 0; i < size; i++) {
                auto v = asms[i];
                if (v) vec.push_back(v);
            }

            return vec;
        }
    };
}