#pragma once

#include <string>
#include <string_view>
#include <format>
#include <android/log.h>


namespace Logcat {
    template<typename... Args>
    void Print(int priority, std::string_view tag, std::format_string<Args...> fmt, Args&&... args) {
        std::string tagg(tag);
        std::string msg = std::format(fmt, std::forward<Args>(args)...);
        __android_log_write(priority, tagg.c_str(), msg.c_str());
    }

    template<typename... Args>
    void Debug(std::string_view tag, std::format_string<Args...> fmt, Args&&... args) {
        Print(ANDROID_LOG_DEBUG, tag, fmt, std::forward<Args>(args)...);
    }

    template<typename... Args>
    void Verbose(std::string_view tag, std::format_string<Args...> fmt, Args&&... args) {
        Print(ANDROID_LOG_VERBOSE, tag, fmt, std::forward<Args>(args)...);
    }

    template<typename... Args>
    void Info(std::string_view tag, std::format_string<Args...> fmt, Args&&... args) {
        Print(ANDROID_LOG_INFO, tag, fmt, std::forward<Args>(args)...);
    }

    template<typename... Args>
    void Warn(std::string_view tag, std::format_string<Args...> fmt, Args&&... args) {
        Print(ANDROID_LOG_WARN, tag, fmt, std::forward<Args>(args)...);
    }

    template<typename... Args>
    void Error(std::string_view tag, std::format_string<Args...> fmt, Args&&... args) {
        Print(ANDROID_LOG_ERROR, tag, fmt, std::forward<Args>(args)...);
    }

    template<typename... Args>
    void Fatal(std::string_view tag, std::format_string<Args...> fmt, Args&&... args) {
        Print(ANDROID_LOG_FATAL, tag, fmt, std::forward<Args>(args)...);
    }
}