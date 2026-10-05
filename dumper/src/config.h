#include <set>
#include <fstream>
#include <filesystem>
#include <nlohmann/json.hpp>
#include "resolver.h"


namespace Config {
    struct DumperConfig {
        std::string libName = "libil2cpp.so";
        int sleepTime = 5;
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
            Extract(j, "lib_name", config.libName);
            Extract(j, "sleep_time", config.sleepTime);
            Extract(j, "filter_assembly", config.filterAssembly);
            Extract(j, "assmbly_filter", config.assmblyFilter);
            Extract(j, "trigger_by_hook", config.triggerByHook);
            Extract(j, "class_blacklist", config.classBlacklist);
            Extract(j, "split_dump", config.splitDump);
        }

        nlohmann::ordered_json ToJson() {
            return {
                {"lib_name", libName},
                {"sleep_time", sleepTime},
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
                for (const auto& key : {"lib_name", "filter_assembly", "assmbly_filter", "trigger_by_hook", "class_blacklist", "sleep_time", "split_dump"}) {
                    if (j.contains(key)) continue;
                    missing = true;
                    break;
                }

                FromJson(j, config);

                if (missing) {
                    std::ofstream out(path);
                    out << config.ToJson().dump(4) << '\n';
                }
            } else {
                std::ofstream out(path);
                out << config.ToJson().dump(4) << '\n';
            }

            return config;
        }
    };

    std::unique_ptr<ApiResolver> GetApiResolver(const std::filesystem::path& extDir) {
        auto mapFileP = extDir / "symbol_map.txt";
        auto rvaFileP = extDir / "rva_map.txt";

        if (std::filesystem::exists(mapFileP)) {
            std::map<std::string, std::string> symMap;
            Log::Debug("symbol_map.txt found");

            std::ifstream mapF(mapFileP);

            std::string line;
            while (std::getline(mapF, line)) {
                std::istringstream ss(line);

                std::string key;
                std::string value;
                ss >> key >> value;

                if (key.empty()) {
                    Log::Debug("key is empty");
                    continue;
                }
                if (value.empty()) {
                    Log::Debug("value is empty");
                    continue;
                }

                symMap[key] = value;
            }

            Log::Debug("loaded {:d} keys", symMap.size());
            return std::make_unique<SymApiResolver>(symMap);
        }

        if (std::filesystem::exists(rvaFileP)) {
            std::map<std::string, size_t> offMap;
            Log::Debug("rva_map.txt found");

            std::ifstream mapF(rvaFileP);

            std::string line;
            while (std::getline(mapF, line)) {
                std::istringstream ss(line);

                std::string key;
                std::string value;
                ss >> key >> value;

                if (key.empty()) {
                    Log::Debug("key is empty");
                    continue;
                }
                if (value.empty()) {
                    Log::Debug("value is empty");
                    continue;
                }

                try {
                    auto offset = static_cast<size_t>(std::stoull(value, nullptr, 16));
                    offMap[key] = offset;
                } catch (const std::exception& e) {
                    Log::Debug("invalid offset for {}: {}", key, value);
                    return nullptr;
                }
            }

            Log::Debug("loaded {:d} keys", offMap.size());
            return std::make_unique<RvaApiResolver>(offMap);
        }

        return std::make_unique<SymApiResolver>();
    }

    std::map<uintptr_t, uintptr_t> GetStaticWriteMap(const std::filesystem::path& extDir) {
        auto mapFileP = extDir / "static_write_map.txt";

        if (std::filesystem::exists(mapFileP)) {
            std::map<uintptr_t, uintptr_t> out;
            Log::Debug("static_write_map.txt found");

            std::ifstream mapF(mapFileP);

            std::string line;
            while (std::getline(mapF, line)) {
                std::istringstream ss(line);

                std::string key;
                std::string value;
                ss >> key >> value;

                if (key.empty()) {
                    Log::Debug("key is empty");
                    continue;
                }
                if (value.empty()) {
                    Log::Debug("value is empty");
                    continue;
                }

                size_t offset = 0;
                try {
                    offset = static_cast<size_t>(std::stoull(key, nullptr, 16));
                } catch (const std::exception& e) {
                    Log::Debug("invalid write offset: {}", key);
                    return {};
                }

                uintptr_t val = 0;
                try {
                    val = static_cast<size_t>(std::stoull(value, nullptr, 16));
                } catch (const std::exception& e) {
                    Log::Debug("invalid write value for 0x{:X}: {}", offset, value);
                    return {};
                }

                out[offset] = val;
            }

            Log::Debug("loaded {:d} static write offsets", out.size());
            return out;
        }
        return {};
    }
}