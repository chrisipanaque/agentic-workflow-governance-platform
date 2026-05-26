#include "ownership_validator.hpp"
#include <iostream>
#include <fstream>
#include <nlohmann/json.hpp>

using json = nlohmann::json;

OwnershipValidator::OwnershipValidator(const std::string& config_path)
    : config_path_(config_path) {}

void OwnershipValidator::load_rules() {
    std::ifstream file(config_path_);
    if (!file.is_open()) {
        throw std::runtime_error("Cannot open ownership rules: " + config_path_);
    }

    json config;
    file >> config;

    for (const auto& b : config["boundaries"]) {
        Boundary boundary;
        boundary.name = b["name"];
        boundary.team = b["team"];

        for (const auto& dir : b["directories"]) {
            boundary.directories.push_back(dir);
        }

        boundaries_.push_back(std::move(boundary));
    }
}

bool OwnershipValidator::path_matches_directory(const std::string& path,
                                                  const std::string& directory) const {
    return path.find(directory) == 0;
}

std::vector<OwnershipValidator::OwnershipInfo> OwnershipValidator::get_file_ownership(
    const DiffScanner::DiffStats& diff_stats) {
    std::vector<OwnershipInfo> result;

    for (const auto& file : diff_stats.files) {
        bool found = false;
        for (const auto& boundary : boundaries_) {
            for (const auto& dir : boundary.directories) {
                if (path_matches_directory(file.file_path, dir)) {
                    result.push_back({file.file_path, boundary.name, boundary.team});
                    found = true;
                    break;
                }
            }
            if (found) break;
        }
        if (!found) {
            result.push_back({file.file_path, "unowned", ""});
        }
    }

    return result;
}

std::vector<std::string> OwnershipValidator::detect_cross_boundary(
    const DiffScanner::DiffStats& diff_stats) {
    auto ownership = get_file_ownership(diff_stats);
    std::vector<std::string> touched_boundaries;

    for (const auto& info : ownership) {
        if (info.boundary_name == "unowned") continue;

        bool found = false;
        for (const auto& existing : touched_boundaries) {
            if (existing == info.boundary_name) {
                found = true;
                break;
            }
        }
        if (!found) {
            touched_boundaries.push_back(info.boundary_name);
        }
    }

    return touched_boundaries;
}

void OwnershipValidator::print_ownership(const std::vector<OwnershipInfo>& ownership_info) const {
    std::cerr << "TRACE: File ownership:" << std::endl;
    for (const auto& info : ownership_info) {
        std::cerr << "TRACE:   " << info.file << " → " << info.boundary_name;
        if (!info.team.empty()) {
            std::cerr << " (" << info.team << ")";
        }
        std::cerr << std::endl;
    }
}

void OwnershipValidator::print_cross_boundary(const std::vector<std::string>& touched_boundaries) const {
    if (touched_boundaries.size() <= 1) {
        std::cerr << "TRACE: Cross-boundary check: PASS (all changes within same boundary)" << std::endl;
        return;
    }

    std::cerr << "TRACE: Cross-boundary check: WARNING" << std::endl;
    std::cerr << "TRACE: PR touches multiple ownership boundaries:" << std::endl;
    for (const auto& boundary : touched_boundaries) {
        std::cerr << "TRACE:   - " << boundary << std::endl;
    }
}
