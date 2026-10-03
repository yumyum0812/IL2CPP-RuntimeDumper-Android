#pragma once
#include <string_view>
#include <vector>
#include "api/domain.h"
#include "api/assembly.h"
#include "api/image.h"
#include "api/type.h"
#include "api/class.h"
#include "api/method.h"

namespace Finder {
    Api::Image* FindImage(std::string_view name) {
        auto domain = Api::Domain::GetInstance();
        auto asms = domain->GetAssemblies();
        for (auto a : asms) {
            auto img = a->GetImage();
            if (img->GetName() == name)
                return img;
        }
        return nullptr;
    }

    Api::Method* FindMethod(Api::Class* klass, std::string_view name, std::string_view rtName, const std::vector<std::string_view>& pNames) {
        if (!klass) return nullptr;

        auto methods = klass->GetMethods();

        for (auto m : methods) {
            if (m->GetName() != name) continue;

            if (!rtName.empty()) {
                auto rt = m->GetReturnType();
                if (!rt || rt->GetName() != rtName) continue;
            }

            auto pc = m->GetParamCount();
            if (pc != pNames.size()) continue;

            bool ok = true;
            for (int i = 0; i < pc; i++) {
                auto c = m->GetParam(i);
                if (!c || c->GetName() != pNames[i]) {
                    ok = false;
                    break;
                }
            }

            if (ok) return m;
        }

        return nullptr;
    }

    Api::Method* FindMethodP(Api::Class* klass, std::string_view name, std::string_view rtName, const std::vector<std::string_view>& pNames) {
        if (!klass) return nullptr;

        auto m = FindMethod(klass, name, rtName, pNames);
        if (m) return m;

        auto par = klass->GetParent();
        if (par && par != klass)
            return FindMethodP(par, name, rtName, pNames);
        return nullptr;
    }

    Api::Method* FindMethodI(Api::Class* klass, std::string_view name, std::string_view rtName, const std::vector<std::string_view>& pNames) {
        if (!klass) return nullptr;

        auto m = FindMethod(klass, name, rtName, pNames);
        if (m) return m;

        auto inters = klass->GetInterfaces();
        for (auto p : inters) {
            if (!p || p == klass) continue;
            return FindMethodI(p, name, rtName, pNames);
        }
        return nullptr;
    }
}