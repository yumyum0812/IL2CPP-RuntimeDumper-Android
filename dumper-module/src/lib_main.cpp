#include <cstddef>
#include <fstream>
#include <string>
#include <thread>
#include <fcntl.h>
#include <unistd.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <jni.h>
#include <android/log.h>
#include <zygisk.hpp>
#include <dumper.h>
#include "module_log.h"

static constexpr auto PACKAGES_CONFIG = "/data/adb/runtimedumper-packages.txt";


static ssize_t xread(int fd, void *buffer, size_t countToRead) {
    ssize_t totalRead = 0;
    char *currentBuf = static_cast<char *>(buffer);
    size_t remainingBytes = countToRead;

    while (remainingBytes > 0) {
        const ssize_t ret = TEMP_FAILURE_RETRY(read(fd, currentBuf, remainingBytes));
        if (ret < 0) {
            return -1;
        }
        if (ret == 0) {
            break;
        }

        currentBuf += ret;
        totalRead += ret;
        remainingBytes -= ret;
    }

    return totalRead;
}

static ssize_t xwrite(int fd, const void *buffer, size_t countToWrite) {
    ssize_t totalWritten = 0;
    const char *currentBuf = static_cast<const char *>(buffer);
    size_t remainingBytes = countToWrite;

    while (remainingBytes > 0) {
        const ssize_t ret = TEMP_FAILURE_RETRY(write(fd, currentBuf, remainingBytes));
        if (ret < 0) {
            return -1;
        }
        if (ret == 0) {
            break;
        }

        currentBuf += ret;
        totalWritten += ret;
        remainingBytes -= ret;
    }

    return totalWritten;
}


static bool ReadExact(int fd, void *buffer, size_t size) {
    return xread(fd, buffer, size) == static_cast<ssize_t>(size);
}

static bool WriteExact(int fd, const void *buffer, size_t size) {
    return xwrite(fd, buffer, size) == static_cast<ssize_t>(size);
}


static void ApplySocketTimeout(int fd) {
    constexpr int PAYLOAD_TIMEOUT_MS = 5000;

    const timeval timeout{
        .tv_sec = PAYLOAD_TIMEOUT_MS / 1000,
        .tv_usec = static_cast<suseconds_t>((PAYLOAD_TIMEOUT_MS % 1000) * 1000),
    };

    setsockopt(fd, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout));
    setsockopt(fd, SOL_SOCKET, SO_SNDTIMEO, &timeout, sizeof(timeout));
}


static void CompanionHandler(int fd) {
    uint32_t len = 0;
    uint8_t result = 0;

    if (!ReadExact(fd, &len, sizeof(len))) {
        close(fd);
        return;
    }

    if (len == 0 || len > 511) {
        close(fd);
        return;
    }

    std::string name(len, '\0');

    if (!ReadExact(fd, name.data(), len)) {
        close(fd);
        return;
    }

    std::ifstream file(PACKAGES_CONFIG);

    if (file.is_open()) {
        std::string line;

        while (std::getline(file, line)) {
            if (!line.empty() && line.back() == '\r')
                line.pop_back();

            if (line == name) {
                result = 1;
                break;
            }
        }
    }

    WriteExact(fd, &result, sizeof(result));
    close(fd);
}

static void StartDumper() {
    std::thread thr(RunDumper);
    thr.detach();
}

class MyModule : public zygisk::ModuleBase {
    zygisk::Api* api = nullptr;
    JNIEnv* env = nullptr;
    bool matched = false;
    std::string processName;

public:
    void onLoad(zygisk::Api* api, JNIEnv* env) override {
        this->api = api;
        this->env = env;
    }

    void preAppSpecialize(zygisk::AppSpecializeArgs* args) override {
        matched = false;

        if (!args) return;
        if (!args->nice_name) return;
        if (!api) return;
        if (!env) return;

        // ModuleLog::Debug("{} {} {} {} {}",
        //     args->uid,
        //     args->is_child_zygote ? *args->is_child_zygote : false,
        //     args->is_child_zygote != nullptr,
        //     args->is_top_app ? *args->is_top_app : false,
        //     args->is_top_app != nullptr
        // );

        if (args->uid < 10000) return; // SYSTEM APP
        if (args->is_child_zygote != nullptr && *args->is_child_zygote) return; // BACKGROUND
        if (args->is_top_app != nullptr && !(*args->is_top_app))  return; // BACKGROUND 2

        const char* raw = env->GetStringUTFChars(args->nice_name, nullptr);
        if (!raw) {
            ModuleLog::Error("cannot get nice_name");
            return;
        };

        processName = raw;
        env->ReleaseStringUTFChars(args->nice_name, raw);

        int companionFd = api->connectCompanion();
        if (companionFd < 0) {
            ModuleLog::Error("cannot connect companion");
            return;
        }

        uint32_t nameLen = processName.size();
        if (!WriteExact(companionFd, &nameLen, sizeof(nameLen)) ||
            !WriteExact(companionFd, processName.data(), nameLen)) {
            close(companionFd);
            return;
        }

        uint8_t shouldProceed = 0;
        if (ReadExact(companionFd, &shouldProceed, sizeof(shouldProceed))) {
            if (shouldProceed == 1) {
                this->matched = true;
            }
        }

        close(companionFd);

        if (!matched) {
            api->setOption(zygisk::Option::DLCLOSE_MODULE_LIBRARY);
            return;
        }

        ModuleLog::Info("app matched: {}", processName);
    }

    void postAppSpecialize(const zygisk::AppSpecializeArgs* args) override {
        if (!matched) return;

        ModuleLog::Info("loaded: pid={}, uid={}, name={}", (int) getpid(), (int) getuid(), processName);
        StartDumper();
    }
};


REGISTER_ZYGISK_MODULE(MyModule)
REGISTER_ZYGISK_COMPANION(CompanionHandler)