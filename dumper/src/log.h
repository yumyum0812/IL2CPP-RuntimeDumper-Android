#pragma once

#include <string>
#include <format>
#include <fstream>
#include <chrono>
#include <android/log.h>


namespace Log {
    constexpr auto TAG = "RuntimeDumper";

    inline std::ofstream _file;
    inline std::mutex _mtx;

    inline void SetFile(const std::string& path) {
        std::lock_guard lock(_mtx);

        if (_file.is_open())
            _file.close();

        _file.open(path, std::ios::app);
    }

    inline void CloseFile() {
        std::lock_guard lock(_mtx);

        if (_file.is_open())
            _file.close();
    }
    
    template<typename... Args>
    inline void _Print(int priority, std::format_string<Args...> fmt, std::string_view modeTag, Args&&... args) {
        std::string msg = std::format(fmt, std::forward<Args>(args)...);
        __android_log_write(priority, TAG, msg.c_str());

        if (_file.is_open()) {
            auto now = std::chrono::system_clock::now();
            auto sec = std::chrono::duration<double>(
                now - std::chrono::floor<std::chrono::minutes>(now)
            ).count();
            auto timestamp = std::format("{:%Y-%m-%d %H:%M}:{:06.3f}", now, sec);

            _file << timestamp;
            _file << " [";
            _file << modeTag;
            _file << "] ";
            _file << msg;
            _file << '\n';
            _file.flush();
        }
    }

    template<typename... Args>
    inline void Debug(std::format_string<Args...> fmt, Args&&... args) {
        _Print(ANDROID_LOG_DEBUG, fmt, "D", std::forward<Args>(args)...);
    }

    template<typename... Args>
    inline void Verbose(std::format_string<Args...> fmt, Args&&... args) {
        _Print(ANDROID_LOG_VERBOSE, fmt, "D", std::forward<Args>(args)...);
    }

    template<typename... Args>
    inline void Info(std::format_string<Args...> fmt, Args&&... args) {
        _Print(ANDROID_LOG_INFO, fmt, "I", std::forward<Args>(args)...);
    }

    template<typename... Args>
    inline void Warn(std::format_string<Args...> fmt, Args&&... args) {
        _Print(ANDROID_LOG_WARN, fmt, "W", std::forward<Args>(args)...);
    }

    template<typename... Args>
    inline void Error(std::format_string<Args...> fmt, Args&&... args) {
        _Print(ANDROID_LOG_ERROR, fmt, "E", std::forward<Args>(args)...);
    }

    template<typename... Args>
    inline void Fatal(std::format_string<Args...> fmt, Args&&... args) {
        _Print(ANDROID_LOG_FATAL, fmt, "F", std::forward<Args>(args)...);
    }
}