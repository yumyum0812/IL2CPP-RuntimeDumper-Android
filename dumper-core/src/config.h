#pragma once
#include <set>
#include <fstream>
#include <filesystem>
#include <nlohmann/json.hpp>
#include <stdexcept>
#include "nlohmann/json_fwd.hpp"
#include "resolver.h"


namespace Config {
    struct DumperConfig {
        bool noDump = false;
        std::string libName = "libil2cpp.so";
        int waitTime = 5;
        bool filterAssembly = false;
        std::set<std::string> assmblyFilter;
        std::set<std::string> classBlacklist;
        bool triggerByHook = false;
        bool splitDump = false;

    private:
        template <typename T>
        static void Extract(const nlohmann::ordered_json& j, std::string_view key, T& v) {
            if (j.contains(key))
                j.at(key).get_to(v);
        }

    public:
        static void FromJson(const nlohmann::ordered_json& j, DumperConfig& config) {
            Extract(j, "no_dump", config.noDump);
            Extract(j, "lib_name", config.libName);
            Extract(j, "wait_time", config.waitTime);
            Extract(j, "filter_assembly", config.filterAssembly);
            Extract(j, "assmbly_filter", config.assmblyFilter);
            Extract(j, "trigger_by_hook", config.triggerByHook);
            Extract(j, "class_blacklist", config.classBlacklist);
            Extract(j, "split_dump", config.splitDump);
        }

        nlohmann::ordered_json ToJson() {
            return {
                {"no_dump", noDump},
                {"lib_name", libName},
                {"wait_time", waitTime},
                {"filter_assembly", filterAssembly},
                {"assmbly_filter", assmblyFilter},
                {"trigger_by_hook", triggerByHook},
                {"class_blacklist", classBlacklist},
                {"split_dump", splitDump}
            };
        }

        static DumperConfig Load(const std::filesystem::path& path) {
            DumperConfig config;
            nlohmann::json j;

            std::ifstream file(path);

            if (file) {
                file >> j;

                bool missing = false;
                for (const auto& key : {"no_dump", "lib_name", "filter_assembly", "assmbly_filter", "trigger_by_hook", "class_blacklist", "wait_time", "split_dump"}) {
                    if (j.contains(key)) continue;
                    missing = true;
                    break;
                }

                FromJson(j, config);

                if (missing) {
                    std::ofstream out(path);
                    out << config.ToJson().dump(4) << '\n';
                    DumperLog::Debug("created dumper config", path.filename().string());
                }
            } else {
                std::ofstream out(path);
                out << config.ToJson().dump(4) << '\n';
            }

            return config;
        }
    };

    inline int ParseJsonInt(nlohmann::json v) {
        if (v.is_number_integer())
            return v.get<int>();

        if (v.is_string()) {
            std::string value = v.get<std::string>();
            std::size_t pos = 0;
            
            int n;
            try {
                n = std::stoi(value, &pos, 0);
            } catch (const std::exception& e) {
                throw std::runtime_error(std::format("invalid integer: {}", e.what()));
            }

            if (pos != value.size()) {
                throw std::runtime_error("invalid integer");
            }

            return n;
        }

        throw std::runtime_error(std::format("invalid value type: {}", v.type_name()));
    }

    inline std::vector<uint8_t> ParseHexBytes(std::string_view hex) {
        std::vector<uint8_t> result;

        std::string compact;
        compact.reserve(hex.size());

        for (char c : hex) {
            if (c == ' ' || c == '\t' || c == '\n' || c == '\r') {
                continue;
            }
            compact.push_back(c);
        }

        if (compact.size() % 2 != 0) {
            throw std::invalid_argument("hex string is odd");
        }

        result.reserve(compact.size() / 2);

        for (std::size_t i = 0; i < compact.size(); i += 2) {
            uint8_t byte = 0;

            auto first = compact.data() + i;
            auto last  = first + 2;

            unsigned int value = 0;
            auto [ptr, ec] = std::from_chars(first, last, value, 16);

            if (ec != std::errc{} || ptr != last || value > 0xFF) {
                throw std::invalid_argument("invalid hex string: position " + std::to_string(i));
            }

            byte = static_cast<uint8_t>(value);
            result.push_back(byte);
        }

        return result;
    }

    struct PatchInfo {
        size_t offset{};
        std::vector<uint8_t> bytes{};

        static PatchInfo Load(const nlohmann::json& j) {
            try {
                if (!j.is_object()) {
                    throw std::runtime_error(std::format("root must be root"));
                }

                auto offV = j.at("offset");
                auto hexS = j.at("bytes").get<std::string>();

                int off = ParseJsonInt(offV);
                std::vector<uint8_t> bytes = ParseHexBytes(hexS);

                PatchInfo p;
                p.offset = off;
                p.bytes = bytes;

                return p;
            } catch (const nlohmann::json::exception& e) {
                throw std::runtime_error(std::format("failed to format: {}", e.what()));
            }
        }
    };

    inline ApiResolver LoadApiResolver(const std::filesystem::path& dir) {
        auto symMapP = dir / "symbol_map.json";
        auto rvaMapP = dir / "rva_map.json";

        ApiResolver ar;

        if (std::filesystem::exists(symMapP)) {
            std::map<std::string, std::string> result;
            DumperLog::Debug("symbol_map.json found");

            std::ifstream f(symMapP);

            nlohmann::json j;

            try {
                f >> j;
            } catch (const nlohmann::json::exception& e) {
                throw std::runtime_error(std::format("failed to parse: {}", e.what()));
            }

            if (!j.is_object()) {
                throw std::runtime_error("root must be an object");
            }

            for (auto it = j.begin(); it != j.end(); ++it) {
                if (it.value().is_string()) {
                    result[it.key()] = it.value().get<std::string>();
                } else {
                    throw std::runtime_error("invalid value type: " + it.key());
                }
            }

            ar.symbolMap = result;
            DumperLog::Debug("symbol map: loaded {:d} keys", result.size());
        }

        if (std::filesystem::exists(rvaMapP)) {
            std::map<std::string, size_t> result;
            DumperLog::Debug("rva_map.json found");

            std::ifstream f(rvaMapP);

            nlohmann::json j;

            try {
                f >> j;
            } catch (const nlohmann::json::exception& e) {
                throw std::runtime_error(std::format("failed to parse: {}", e.what()));
            }

            if (!j.is_object()) {
                throw std::runtime_error("root must be an object");
            }

            for (auto it = j.begin(); it != j.end(); ++it) {
                result[it.key()] = ParseJsonInt(it.value());
            }

            DumperLog::Debug("symbol map: loaded {:d} keys", result.size());
            ar.rvaMap = result;
        }

        return ar;
    }

    inline std::vector<PatchInfo> LoadPatches(const std::filesystem::path& dir) {
        auto mapFileP = dir / "patches.json";

        if (!std::filesystem::exists(mapFileP)) {
            return {};
        }
    
        DumperLog::Debug("patches.txt found");

        std::ifstream mapF(mapFileP);
        nlohmann::json j;
        try {
            mapF >> j;
        } catch (const nlohmann::json::exception& e) {
            throw std::runtime_error(std::format("failed to parse: {}", e.what()));
        }

        if (!j.is_array()) {
            throw std::runtime_error("root must be an array");
        }

        std::vector<PatchInfo> out;
        for (std::size_t i = 0; i < j.size(); ++i) {
            auto p = PatchInfo::Load(j[i]);
            out.push_back(p);
        }

        DumperLog::Debug("loaded {:d} patches", out.size());
        return out;
    }
}