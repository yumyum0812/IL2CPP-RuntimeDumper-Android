#include <vector>
#include <filesystem>
#include <set>

namespace StrUtil {
    std::string Join(const std::vector<std::string>& vars, const std::string& delim) {
        std::string out;
        for (int i = 0; i < vars.size(); i++) {
            if (i >= 1) out += delim;
            out += vars[i];
        }
        return out;
    }

    std::string Join(const std::set<std::string>& vars, const std::string& delim) {
        std::string out;
        for (auto it = vars.begin(); it != vars.end(); ++it) {
            if (it != vars.begin()) out += delim;
            out += *it;
        }
        return out;
    }

    std::string Repeat(const std::string& str, int n) {
        std::string out;
        for (int i = 0; i < n; i++) {
            out += str;
        }
        return out;
    }

    void EraseSuffix(std::string& str, std::string_view suff) {
        if (str.ends_with(suff))
            str.erase(str.size() - suff.size());
    }

    std::string SubBefore(const std::string str, const std::string& delim) {
        size_t pos = str.find(delim);
        return (pos != std::string::npos) ? str.substr(0, pos) : str;
    }

    std::string U16ToU8(const std::u16string& u16str) {
        std::u8string u8str = std::filesystem::path(u16str).u8string();
        return std::string(u8str.begin(), u8str.end());
    }
}