#pragma once
#include <string>
#include <map>
#include <format>

namespace Formats {
    std::string FormatCharLiteral(uint16_t v) {
        std::string out;
        if (std::iscntrl(v)) {
            out = "\\x" + std::format("{:X}", v);
        } else if (v == '\'') {
            out = "\\'";
        } else {
            out = std::string(1, v);
        }

        return "'" + out + "'";
    }
    std::string FormalStringLiteral(std::string_view input) {
        const static std::map<char, std::string> charEscapes = {
            {'\\', "\\\\"},
            {'"',  "\\\""},
            {'\n', "\\n"},
            {'\0', "\\0"},
            {'\a', "\\a"},
            {'\f', "\\f"},
            {'\n', "\\n"},
            {'\r', "\\r"},
            {'\t', "\\t"},
            {'\v', "\\v"}
        };

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