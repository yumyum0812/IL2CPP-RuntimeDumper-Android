#pragma once

#include <string>
#include <format>
#include <android/log.h>


namespace ModuleLog {
    inline constexpr auto TAG = "ZygiskRuntimeDumper";
    
    template<typename... Args>
    void _Print(android_LogPriority priority, char modeTag, std::format_string<Args...> fmt, Args&&... args) {
        std::string msg = std::format(fmt, std::forward<Args>(args)...);
        __android_log_write(priority, TAG, msg.c_str());
    }

    template<typename... Args>
    [[noreturn]]
    void _Assert(const std::string& cond, char modeTag, std::format_string<Args...> fmt, Args&&... args) {
        std::string msg = std::format(fmt, std::forward<Args>(args)...);
        __android_log_assert(cond.c_str(), TAG, "%s", msg.c_str());
    }

    template<typename... Args>
    void Debug(std::format_string<Args...> fmt, Args&&... args) {
        _Print(ANDROID_LOG_DEBUG, 'D', fmt, std::forward<Args>(args)...);
    }

    template<typename... Args>
    void Verbose(std::format_string<Args...> fmt, Args&&... args) {
        _Print(ANDROID_LOG_VERBOSE, 'V', fmt, std::forward<Args>(args)...);
    }

    template<typename... Args>
    void Info(std::format_string<Args...> fmt, Args&&... args) {
        _Print(ANDROID_LOG_INFO, 'I', fmt, std::forward<Args>(args)...);
    }

    template<typename... Args>
    void Warn(std::format_string<Args...> fmt, Args&&... args) {
        _Print(ANDROID_LOG_WARN, 'W', fmt, std::forward<Args>(args)...);
    }

    template<typename... Args>
    void Error(std::format_string<Args...> fmt, Args&&... args) {
        _Print(ANDROID_LOG_ERROR, 'E', fmt, std::forward<Args>(args)...);
    }

    template<typename... Args>
    void Fatal(std::format_string<Args...> fmt, Args&&... args) {
        _Print(ANDROID_LOG_FATAL, 'F', fmt, std::forward<Args>(args)...);
    }

    template<typename... Args>
    [[noreturn]]
    void Assert(const std::string& cond, std::format_string<Args...> fmt, Args&&... args) {
        _Assert(cond, 'A', fmt, std::forward<Args>(args)...);
    }
}