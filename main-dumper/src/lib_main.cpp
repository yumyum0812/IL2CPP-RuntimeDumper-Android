#include <string_view>
#include <format>
#include <thread>
#include <filesystem>
#include <dlfcn.h>
#include <jni.h>
#include "dumper.h"


std::vector<void*> gHandles;


void StartDumper() {
    auto t = std::thread(RunDumper);
    t.detach();
}

bool OpenLib(JNIEnv* env, const std::filesystem::path& dir, std::string_view libName) {
    auto fullPath = dir / libName;

    void* handle = dlopen(fullPath.c_str(), RTLD_NOW);
    if (!handle) {
        auto err = dlerror();
        auto msg = std::format("Unable to load library: {} [{}]", fullPath.c_str(), err);
        env->FatalError(msg.c_str());
        return false;
    }

    auto onLoadFn = reinterpret_cast<jint (*)(JavaVM*, void**)>(dlsym(handle, "JNI_OnLoad"));
    if (onLoadFn) {
        JavaVM* vm;
        if (env->GetJavaVM(&vm) != JNI_OK) {
            env->FatalError("Unable to retrieve Java VM");
            return false;
        }

        jint res = onLoadFn(vm, nullptr);
        if (res > JNI_VERSION_1_6) {
            env->FatalError("Unsupported VM version");
            return false;
        }
    }

    gHandles.push_back(handle);

    return true;
}

jboolean Load(JNIEnv* env, jclass clazz, jstring path) {
    const char* libDir = env->GetStringUTFChars(path, nullptr);
    if (!libDir) {
        return JNI_FALSE;
    }

    std::filesystem::path libDirPath(libDir);

    env->ReleaseStringUTFChars(path, libDir);
    
    bool ok = true;
    ok &= OpenLib(env, libDirPath, "libunity.so");
    ok &= OpenLib(env, libDirPath, "libil2cpp.so");
    return ok;
}

jboolean Unload(JNIEnv* env, jclass clazz) {
    JavaVM* vm;
    if (env->GetJavaVM(&vm) != JNI_OK) {
        env->FatalError("Unable to retrive Java VM");
        return JNI_FALSE;
    }

    for (void* handle : gHandles) {
        auto unLoadFn = reinterpret_cast<jint (*)(JavaVM*, void**)>(dlsym(handle, "JNI_OnLoad"));
        if (unLoadFn) {
            unLoadFn(vm, nullptr);
        }
        dlclose(handle);
    }
    gHandles.clear();

    return true;
}


JNIEXPORT jint JNICALL
JNI_OnLoad(JavaVM* vm, void*) {
    JNIEnv *globalEnv;
    if (vm->GetEnv(reinterpret_cast<void**>(&globalEnv), JNI_VERSION_1_6) != JNI_OK) {
        return JNI_ERR;
    }

    jclass clazz = globalEnv->FindClass("com/unity3d/player/NativeLoader");
    if (!clazz) {
        return JNI_ERR;
    }

    JNINativeMethod methods[] = {
        {"load",   "(Ljava/lang/String;)Z", reinterpret_cast<void *>(Load)},
        {"unload", "()Z",                   reinterpret_cast<void *>(Unload)}
    };

    jint ret = globalEnv->RegisterNatives(clazz, methods, sizeof(methods) / sizeof(JNINativeMethod));
    if (ret != JNI_OK) {
        return ret;
    }

    StartDumper();

    return JNI_VERSION_1_6;
}