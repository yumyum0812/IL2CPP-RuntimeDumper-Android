#pragma once
#include <cctype>
#include <string>
#include <map>
#include <format>
#include "utils/str_util.h"

namespace Formats {
    inline const static std::map<char16_t, std::string> charEscapes = {
        {u'\\', "\\\\"},
        {u'\n', "\\n"},
        {u'\0', "\\0"},
        {u'\a', "\\a"},
        {u'\f', "\\f"},
        {u'\n', "\\n"},
        {u'\r', "\\r"},
        {u'\t', "\\t"},
        {u'\v', "\\v"}
    };

    inline std::string FormatCharLiteral(char16_t c) {
        std::string out = [=]() -> std::string {
            if (c == U'\'')
                return "\\\'";

            if (c <= 0xFF && std::iscntrl(static_cast<int>(c)))
                return "\\u" + std::format("{:04X}", static_cast<uint16_t>(c));

            if (c <= 0xFFFF) {
                if (charEscapes.contains(c))
                    return charEscapes.at(c);

                if (c >= 0xFFFE)
                    return "\\u" + std::format("{:04X}", static_cast<uint16_t>(c));
                    
                if (StrUtil::IsLowSurrogate(c) || StrUtil::IsHighSurrogate(c))
                    return "\\u" + std::format("{:04X}", static_cast<uint16_t>(c));
            }

            return StrUtil::EncodeU8Char(static_cast<char32_t>(c));
        }();

        return "'" + out + "'";
    }

    inline std::string FormatStringLiteral(const std::vector<char16_t>& buffer) {
        std::string result;
        StrUtil::DecodeU16(buffer, [&result](char32_t cp) {
            if (cp == U'"') {
                result += "\\\"";
                return;
            }

            if (cp <= 0xFF && std::iscntrl(static_cast<int>(cp))) {
                result += "\\u" + std::format("{:04X}", static_cast<uint16_t>(cp));
                return;
            }

            if (cp <= 0xFFFF) {
                if (charEscapes.contains(static_cast<char16_t>(cp))) {
                    result += charEscapes.at(static_cast<char16_t>(cp));
                    return;
                }
                if (cp >= 0xFFFE) {
                    result += "\\u" + std::format("{:04X}", static_cast<uint16_t>(cp));
                    return;
                }
            }

            result += StrUtil::EncodeU8Char(cp);
        }, [&result](char16_t c) {
            result += "\\u" + std::format("{:04X}", static_cast<uint16_t>(c));
        });
        return "\"" + result + "\"";
    }

    template <std::integral T>
    inline std::string FormatInteger(T value) {
        return std::format("{:d}", value);
    }

    template <std::floating_point T>
    inline std::string FormatFloat(T value) {
        auto s = std::format("{}", value);
        if (!s.empty() &&
            std::all_of(s.begin() + (s.front() == '-' ? 1 : 0), s.end(), [](unsigned char c) {
                return std::isdigit(c);
            })) {
            s += ".0";
        }
        return s;
    }

    template <std::integral T>
    inline std::string FormatOffset(T value) {
        if (value < 0) {
            using U = std::make_unsigned_t<T>;
            const auto magnitude = U{0} - static_cast<U>(value);
            return std::format("-0x{:X}", magnitude);
        }
        return std::format("0x{:X}", value);
    }
}