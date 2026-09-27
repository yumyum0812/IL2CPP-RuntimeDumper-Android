#include "api/functions.h"
#include "api/assembly.h"
#include "api/thread.h"
#include "api/domain.h"
#include <cstdlib>

#include <filesystem>
#include <fstream>


namespace Dumper {
    using Resolver = Api::Functions::Resolver;
    using SymResolver = Api::Functions::SymResolver;

    class RuntimeDumper {
        void CheckKill(bool kill) {
            if (kill) {
                Log::Debug("dumper: aborting...");
                std::abort();
            }
        }
    public:
        void Run(bool killWhenFail = false) {
            using namespace Api;

            auto res = SymResolver();
            bool init = Api::Functions::InitAPI(res);
            if (!init) {
                Log::Debug("dumper: initialization failed");
                CheckKill(killWhenFail);
                return;
            }

            Domain* dmn = Domain::GetInstance();
            Log::Debug("dumper: domain: 0x{:X}", (uintptr_t) dmn);

            Thread* thr = Thread::Attach(dmn);
            Log::Debug("dumper: thread: 0x{:X}", (uintptr_t) thr);

            Image* corlib = Domain::GetCorlib();
            corlib = decltype(corlib)(uintptr_t(corlib) & 0xFFFFFFFFFF);
            Log::Debug("dumper: corlib: 0x{:X}", (uintptr_t) corlib);

            auto nm = corlib->GetName();
            Log::Debug("dumper: corlib: {}", nm);

            auto asms = dmn->GetAssemblies();
            Log::Debug("dumper: assemblies: {:d}", asms.size());

            std::sort(asms.begin(), asms.end(), [](Assembly* a, Assembly*& b) {
                Image* ai = a->GetImage();
                Image* bi = b->GetImage();
                if (!ai) return true;
                if (!bi) return false;
                return ai->GetName() < bi->GetName();
            });

            std::filesystem::path dir("/sdcard/Android/data/com.fengiiley.frlegends/files/dump");
            if (!std::filesystem::exists(dir) && !std::filesystem::create_directories(dir)) {
                Log::Debug("dumper: cannot create dir, {}", dir.c_str());
                CheckKill(killWhenFail);
                return;
            }

            for (auto a : asms) {
                Image* img = a->GetImage();
                if (!img) continue;

                Log::Debug("dumper: asm: {}, {:d}", img->GetName(), img->GetClassCount());
                std::ofstream out(dir / (img->GetName() + ".txt"), std::ios::trunc);

                auto clss = img->GetClasses();
                for (auto c : clss) {
                    Log::Debug("dumper:   {}, {}", c->GetNamespace(), c->GetName());
                    out << c->GetNamespace() + ", " + c->GetName() + "\n";

                    auto mtds = c->GetMethods();
                    for (auto m : mtds) {
                        out << "  " + m->GetName() + "\n";
                    }

                    auto flds = c->GetFields();
                    for (auto f : flds) {
                        out << "    " + f->GetName() + "\n";
                    }
                }

                out.flush();
            }
        }
    };
}