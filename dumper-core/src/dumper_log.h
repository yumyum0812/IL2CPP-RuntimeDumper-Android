#pragma once

#include <string>
#include <format>
#include <fstream>
#include <chrono>
#include <android/log.h>


namespace DumperLog {
    inline constexpr auto TAG = "RuntimeDumper";

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

    inline void _Append(char modeTag, const std::string& msg) {
        std::lock_guard lock(_mtx);

        if (!_file.is_open()) return;
    
        auto now = std::chrono::system_clock::now();
        auto tt = std::chrono::system_clock::to_time_t(now);
        std::tm tm{};
        localtime_r(&tt, &tm);
        
        auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(
            now.time_since_epoch()
        ).count() % 1000;
        
        auto timestamp = std::format(
            "{:04}-{:02}-{:02} {:02}:{:02}.{:03}",
            tm.tm_year + 1900,
            tm.tm_mon + 1,
            tm.tm_mday,
            tm.tm_hour,
            tm.tm_min,
            ms
        );

        _file << timestamp;
        _file << " [";
        _file << modeTag;
        _file << "] ";
        _file << msg;
        _file << '\n';
        _file.flush();
    }
    
    template<typename... Args>
    void _Print(android_LogPriority priority, char modeTag, std::format_string<Args...> fmt, Args&&... args) {
        std::string msg = std::format(fmt, std::forward<Args>(args)...);
        __android_log_write(priority, TAG, msg.c_str());

        _Append(modeTag, msg);
    }

    template<typename... Args>
    [[noreturn]]
    void _Assert(const std::string& cond, char modeTag, std::format_string<Args...> fmt, Args&&... args) {
        std::string msg = std::format(fmt, std::forward<Args>(args)...);
        _Append(modeTag, msg);
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