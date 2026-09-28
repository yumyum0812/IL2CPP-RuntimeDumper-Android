#pragma once
#include "functions.h"
#include "assembly.h"
#include "image.h"

namespace Dumper::Api {
    struct Domain {
        static Domain* GetInstance() {
            return Functions::il2cpp_domain_get();
        }

        static Image* GetCorlib() {
            return Functions::il2cpp_get_corlib();
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

        Assembly* FindAssembly(std::string_view name) {
            auto assemblies = this->GetAssemblies();
            for (auto a : assemblies) {
                auto img = a->GetImage();
                if (!img) continue;

                if (img->GetName() == name)
                    return a;
            }
            return nullptr;
        }
    };
}