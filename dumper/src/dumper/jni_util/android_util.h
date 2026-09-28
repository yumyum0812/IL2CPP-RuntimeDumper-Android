#include <string>
#include <jni.h>

namespace Dumper::AndroidUtil {
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