#include "config_loader.hpp"
#include <nlohmann/json.hpp>
#include <fstream>
#include <stdexcept>

using json = nlohmann::json;

ConfigLoader::ConfigLoader(const std::string& config_path)
    : config_path_(config_path) {}

void ConfigLoader::load() {
    std::ifstream file(config_path_);
    if (!file.is_open()) {
        throw std::runtime_error("Failed to open config file: " + config_path_);
    }

    json config = json::parse(file);
    
    if (config.contains("forbidden_paths") && config["forbidden_paths"].is_array()) {
        for (const auto& item : config["forbidden_paths"]) {
            forbidden_paths_.push_back({
                item["path"].get<std::string>(),
                item["reason"].get<std::string>()
            });
        }
    }
}

const std::vector<ConfigLoader::ForbiddenPath>& ConfigLoader::get_forbidden_paths() const {
    return forbidden_paths_;
}
