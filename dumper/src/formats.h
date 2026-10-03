#pragma once
#include <string>
#include <map>
#include <format>

namespace Formats {
    const static std::map<char, std::string> charEscapes = {
        {'\\', "\\\\"},
        {'\n', "\\n"},
        {'\0', "\\0"},
        {'\a', "\\a"},
        {'\f', "\\f"},
        {'\n', "\\n"},
        {'\r', "\\r"},
        {'\t', "\\t"},
        {'\v', "\\v"}
    };

    std::string FormatCharLiteral(uint16_t c) {
        std::string out;
        if (charEscapes.contains(c)) {
            out += charEscapes.at(static_cast<char>(c));
        } else if (c == '\'') {
            out = "\\'";
        } else if (c < 0x7F && std::iscntrl(c)) {
            out = "\\x" + std::format("{:X}", c);
        } else if ((c >= 0xD800 && c <= 0xDFFF) || (c >= 0xFDD0 && c <= 0xFDEF) || c == 0xFFFE || c == 0xFFFF) {
            out = std::format("\\u{:04X}", c);
        } else {
            out = std::string(1, c);
        }

        return "'" + out + "'";
    }

    std::string FormalStringLiteral(std::string_view input) {
        std::string result;
        for (char c : input) {
            if (charEscapes.contains(c)) {
                result += charEscapes.at(c);
                continue;
            }
            if (std::iscntrl(c)) {
                result += "\\x" + std::to_string(static_cast<int>(static_cast<unsigned char>(c)));
                continue;
            }
            if (c == '"') {
                result += "\\\"";
                continue;
            }
            result += c;
        }
        return "\"" + result + "\"";
    }

    template <std::integral T>
    std::string FormatInteger(T value) {
        return std::format("{:d}", value);
    }

    template <std::floating_point T>
    std::string FormatFloat(T value) {
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
    std::string FormatOffset(T value) {
        if (value < 0) {
            using U = std::make_unsigned_t<T>;
            const auto magnitude = U{0} - static_cast<U>(value);
            return std::format("-0x{:X}", magnitude);
        }
        return std::format("0x{:X}", value);
    }
}