#pragma once
#include <vector>
#include <set>

namespace StrUtil {
    inline std::string Join(const std::vector<std::string>& vars, const std::string& delim) {
        std::string out;
        for (int i = 0; i < vars.size(); i++) {
            if (i >= 1) out += delim;
            out += vars[i];
        }
        return out;
    }

    inline std::string Join(const std::set<std::string>& vars, const std::string& delim) {
        std::string out;
        for (auto it = vars.begin(); it != vars.end(); ++it) {
            if (it != vars.begin()) out += delim;
            out += *it;
        }
        return out;
    }

    inline std::string Repeat(const std::string& str, int n) {
        std::string out;
        for (int i = 0; i < n; i++) {
            out += str;
        }
        return out;
    }

    inline void EraseSuffix(std::string& str, std::string_view suff) {
        if (str.ends_with(suff))
            str.erase(str.size() - suff.size());
    }

    inline std::string SubBefore(const std::string str, const std::string& delim) {
        size_t pos = str.find(delim);
        return (pos != std::string::npos) ? str.substr(0, pos) : str;
    }

    inline bool IsHighSurrogate(char16_t c) {
        return c >= 0xD800 && c < 0xDC00;
    }

    inline bool IsLowSurrogate(char16_t c) {
        return c >= 0xDC00 && c < 0xE000;
    }

    inline void DecodeU16(
        const std::vector<char16_t>& buffer,
        const std::function<void(char32_t)>& okfn,
        const std::function<void(char16_t)>& errFn
    ) {
        std::string result;

        for (size_t i = 0; i < buffer.size(); ++i) {
            char16_t c = buffer[i];
            char32_t cp;

            if (IsHighSurrogate(c)) {
                if (i + 1 < buffer.size() && IsLowSurrogate(buffer[i + 1])) {
                    cp = 0x10000 + ((static_cast<char32_t>(c - 0xD800) << 10) | (buffer[++i] - 0xDC00));
                } else {
                    errFn(c);
                    continue;
                }
            } else if (IsLowSurrogate(c)) {
                errFn(c);
                continue;
            } else {
                cp = c;
            }

            okfn(cp);
        }
    }

    inline std::string EncodeU8Char(char32_t codePoint) {
        std::string s;
        if (codePoint < 0x80) {
            s += static_cast<char>(codePoint);
        } else if (codePoint < 0x800) {
            s += static_cast<char>(0b11000000 | ((codePoint >> 6 ) & 0b00011111));
            s += static_cast<char>(0b10000000 | (codePoint         & 0b00111111));
        } else if (codePoint < 0x10000) {
            s += static_cast<char>(0b11100000 | ((codePoint >> 12) & 0b00001111));
            s += static_cast<char>(0b10000000 | ((codePoint >> 6 ) & 0b00111111));
            s += static_cast<char>(0b10000000 | (codePoint         & 0b00111111));
        } else {
            s += static_cast<char>(0b11110000 | ((codePoint >> 18) & 0b00000111));
            s += static_cast<char>(0b10000000 | ((codePoint >> 12) & 0b00111111));
            s += static_cast<char>(0b10000000 | ((codePoint >> 6 ) & 0b00111111));
            s += static_cast<char>(0b10000000 | (codePoint         & 0b00111111));
        }
        return s;
    }

    inline std::string U16ToU8(const std::vector<char16_t>& buffer) {
        std::string result;
        StrUtil::DecodeU16(buffer, [&result](char32_t cp) {
            result += EncodeU8Char(cp);
        }, {});
        return result;
    }
}