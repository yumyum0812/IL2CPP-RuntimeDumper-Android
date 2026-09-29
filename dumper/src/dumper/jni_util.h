#include <jni.h>
#include <xdl.h>
#include <string>

namespace Dumper::JniUtil {
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

    jobject GetApplication(JNIEnv* env) {
        jclass activityThread = env->FindClass("android/app/ActivityThread");
        if (!activityThread)
            return nullptr;

        jmethodID currentApplication = env->GetStaticMethodID(
            activityThread,
            "currentApplication",
            "()Landroid/app/Application;"
        );

        if (!currentApplication) {
            env->DeleteLocalRef(activityThread);
            return nullptr;
        }

        jobject application = env->CallStaticObjectMethod(
            activityThread,
            currentApplication
        );

        env->DeleteLocalRef(activityThread);

        return application;
    }

    std::string GetExternalFilesPath(JNIEnv* env, jobject context) {
        jclass contextClass = env->GetObjectClass(context);
        if (!contextClass) return {};

        jmethodID getExternalFilesDir = env->GetMethodID(
            contextClass,
            "getExternalFilesDir",
            "(Ljava/lang/String;)Ljava/io/File;"
        );

        if (!getExternalFilesDir) {
            env->DeleteLocalRef(contextClass);
            return {};
        }

        jobject file = env->CallObjectMethod(
            context,
            getExternalFilesDir,
            nullptr
        );

        env->DeleteLocalRef(contextClass);

        if (!file)
            return {};

        jclass fileClass = env->GetObjectClass(file);
        if (!fileClass) {
            env->DeleteLocalRef(file);
            return {};
        }

        jmethodID getPath = env->GetMethodID(
            fileClass,
            "getPath",
            "()Ljava/lang/String;"
        );

        if (!getPath) {
            env->DeleteLocalRef(fileClass);
            env->DeleteLocalRef(file);
            return {};
        }

        jstring path = static_cast<jstring>(env->CallObjectMethod(file, getPath));

        std::string result;

        if (path) {
            const char* chars = env->GetStringUTFChars(path, nullptr);
            if (chars) {
                result.assign(chars);
                env->ReleaseStringUTFChars(path, chars);
            }
            env->DeleteLocalRef(path);
        }

        env->DeleteLocalRef(fileClass);
        env->DeleteLocalRef(file);

        return result;
    }
}