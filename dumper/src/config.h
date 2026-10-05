#include <set>
#include <fstream>
#include <nlohmann/json.hpp>
#include <filesystem>


struct Config {
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
    
    static void FromJson(const nlohmann::ordered_json& j, Config& config) {
        Extract(j, "sleep_time", config.sleepTime);
        Extract(j, "filter_assembly", config.filterAssembly);
        Extract(j, "assmbly_filter", config.assmblyFilter);
        Extract(j, "trigger_by_hook", config.triggerByHook);
        Extract(j, "class_blacklist", config.classBlacklist);
        Extract(j, "split_dump", config.splitDump);
    }

    nlohmann::ordered_json ToJson() {
        return {
            {"sleep_time", sleepTime},
            {"filter_assembly", filterAssembly},
            {"assmbly_filter", assmblyFilter},
            {"trigger_by_hook", triggerByHook},
            {"class_blacklist", classBlacklist},
            {"split_dump", splitDump}
        };
    }

    static Config Load(const std::filesystem::path& path) {
        Config config;
        nlohmann::json j;

        std::ifstream file(path);

        if (file) {
            file >> j;

            bool missing = false;
            for (const auto& key : {"filter_assembly", "assmbly_filter", "trigger_by_hook", "class_blacklist", "sleep_time", "split_dump"}) {
                if (!j.contains(key)) {
                    missing = true;
                    break;
                }
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