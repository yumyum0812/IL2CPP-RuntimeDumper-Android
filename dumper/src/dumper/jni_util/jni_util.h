#include <jni.h>
#include <xdl.h>

namespace Dumper::JniUtil {
    JavaVM* GetJavaVM() {
        void* handle = xdl_open("libart.so", RTLD_NOW | RTLD_GLOBAL);
        if (!handle) return nullptr;

        auto fn = reinterpret_cast<jint(*)(JavaVM**, jsize, jsize*)>(xdl_sym(handle, "JNI_GetCreatedJavaVMs", nullptr));
        if (!fn) return nullptr;

        JavaVM* vm = nullptr;

        jsize ct;
        fn(&vm, 1, &ct);

        xdl_close(handle);

        return vm;
    }

    class JniAttach {
        JavaVM* vm = nullptr;
        JNIEnv* env = nullptr;
        bool owned = false;

    public:
        ~JniAttach() {
                Detach();
        }
        
        JniAttach(JavaVM* vm): vm(vm) {}
        JniAttach(const JniAttach&) = delete;
        JniAttach& operator=(const JniAttach&) = delete;

        JNIEnv* Attach() {
            if (!vm) return nullptr;
            if (env) return env;

            if (vm->GetEnv(reinterpret_cast<void**>(&env), JNI_VERSION_1_6) == JNI_OK) {
                owned = false;
                return env;
            }

            if (vm->AttachCurrentThread(&env, nullptr) != JNI_OK) {
                env = nullptr;
                owned = false;
                return nullptr;
            }

            owned = true;
            return env;
        }

        void Detach() {
            if (owned) {
                vm->DetachCurrentThread();
            }
            owned = false;
            env = nullptr;
        }
    };
}