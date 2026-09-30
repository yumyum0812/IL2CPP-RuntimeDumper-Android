#pragma once
#include "functions.h"

namespace Dumper::Api {
    struct Domain {
        static Domain* GetInstance() {
            return Functions::il2cpp_domain_get();
        }

        std::vector<Assembly*> GetAssemblies() {
            size_t size = 0;
            auto asms = Functions::il2cpp_domain_get_assemblies(this, &size);

            std::vector<Assembly*> vec;
            vec.reserve(size);
            
            for (int i = 0; i < size; i++) {
                vec.push_back(asms[i]);
            }

            return vec;
        }
    };
}