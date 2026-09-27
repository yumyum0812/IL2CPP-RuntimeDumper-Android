#pragma once

#include <string>
#include <format>
#include <android/log.h>


namespace Dumper::Log {
    constexpr auto TAG = "RuntimeDumper";
    
    template<typename... Args>
    void Print(int priority, std::format_string<Args...> fmt, Args&&... args) {
        std::string msg = std::format(fmt, std::forward<Args>(args)...);
        __android_log_write(priority, TAG, msg.c_str());
    }

    template<typename... Args>
    void Debug(std::format_string<Args...> fmt, Args&&... args) {
        Print(ANDROID_LOG_DEBUG, fmt, std::forward<Args>(args)...);
    }

    template<typename... Args>
    void Verbose(std::format_string<Args...> fmt, Args&&... args) {
        Print(ANDROID_LOG_VERBOSE, fmt, std::forward<Args>(args)...);
    }

    template<typename... Args>
    void Info(std::format_string<Args...> fmt, Args&&... args) {
        Print(ANDROID_LOG_INFO, fmt, std::forward<Args>(args)...);
    }

    template<typename... Args>
    void Warn(std::format_string<Args...> fmt, Args&&... args) {
        Print(ANDROID_LOG_WARN, fmt, std::forward<Args>(args)...);
    }

    template<typename... Args>
    void Error(std::format_string<Args...> fmt, Args&&... args) {
        Print(ANDROID_LOG_ERROR, fmt, std::forward<Args>(args)...);
    }

    template<typename... Args>
    void Fatal(std::format_string<Args...> fmt, Args&&... args) {
        Print(ANDROID_LOG_FATAL, fmt, std::forward<Args>(args)...);
    }
}