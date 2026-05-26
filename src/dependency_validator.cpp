#include "dependency_validator.hpp"
#include <iostream>
#include <fstream>
#include <sstream>
#include <nlohmann/json.hpp>

using json = nlohmann::json;

DependencyValidator::DependencyValidator(const std::string& config_path)
    : config_path_(config_path) {}

bool DependencyValidator::is_glob_pattern(const std::string& pattern) const {
    return pattern.find('*') != std::string::npos ||
           pattern.find('?') != std::string::npos;
}

bool DependencyValidator::simple_glob_match(const std::string& text, const std::string& pattern) const {
    size_t text_idx = 0;
    size_t pattern_idx = 0;
    size_t star_idx = std::string::npos;
    size_t match_idx = 0;

    while (text_idx < text.length()) {
        if (pattern_idx < pattern.length()) {
            if (pattern[pattern_idx] == '*') {
                star_idx = pattern_idx;
                match_idx = text_idx;
                pattern_idx++;
                continue;
            } else if (pattern[pattern_idx] == '?' || pattern[pattern_idx] == text[text_idx]) {
                pattern_idx++;
                text_idx++;
                continue;
            }
        }

        if (star_idx != std::string::npos) {
            pattern_idx = star_idx + 1;
            match_idx++;
            text_idx = match_idx;
        } else {
            return false;
        }
    }

    while (pattern_idx < pattern.length() && pattern[pattern_idx] == '*') {
        pattern_idx++;
    }

    return pattern_idx == pattern.length();
}

bool DependencyValidator::path_matches_pattern(const std::string& path, const std::string& pattern) const {
    if (!is_glob_pattern(pattern)) {
        return path.find(pattern) != std::string::npos || path == pattern;
    }
    return simple_glob_match(path, pattern);
}

void DependencyValidator::load_rules() {
    std::ifstream file(config_path_);
    if (!file.is_open()) {
        throw std::runtime_error("Cannot open dependency rules: " + config_path_);
    }

    json config;
    file >> config;

    for (const auto& b : config["boundaries"]) {
        Boundary boundary;
        boundary.name = b["name"];
        boundary.source_pattern = b["source_pattern"];
        boundary.reason = b["reason"];

        for (const auto& dep : b["forbidden_dependencies"]) {
            boundary.forbidden_dependencies.push_back(dep);
        }

        boundaries_.push_back(std::move(boundary));
    }
}

std::vector<std::string> DependencyValidator::extract_includes(const std::string& file_path) const {
    std::vector<std::string> includes;
    std::ifstream file(file_path);
    if (!file.is_open()) {
        return includes;
    }

    std::string line;
    while (std::getline(file, line)) {
        auto inc_pos = line.find("#include");
        if (inc_pos == std::string::npos) continue;

        auto quote_open = line.find('"', inc_pos);
        if (quote_open == std::string::npos) continue;

        auto quote_close = line.find('"', quote_open + 1);
        if (quote_close == std::string::npos) continue;

        auto include_path = line.substr(quote_open + 1, quote_close - quote_open - 1);
        includes.push_back(include_path);
    }

    return includes;
}

bool DependencyValidator::is_forbidden_include(const std::string& include_path,
                                                const std::vector<std::string>& forbidden) const {
    for (const auto& pattern : forbidden) {
        if (include_path.find(pattern) == 0) {
            return true;
        }
    }
    return false;
}

std::vector<DependencyValidator::Violation> DependencyValidator::validate(
    const DiffScanner::DiffStats& diff_stats) {
    std::vector<Violation> violations;

    for (const auto& file : diff_stats.files) {
        // Only check C/C++ source and header files
        auto ext = file.file_path.substr(file.file_path.find_last_of('.'));
        if (ext != ".cpp" && ext != ".hpp" && ext != ".h" &&
            ext != ".cxx" && ext != ".hxx" && ext != ".cc") {
            continue;
        }

        for (const auto& boundary : boundaries_) {
            if (!path_matches_pattern(file.file_path, boundary.source_pattern)) {
                continue;
            }

            auto includes = extract_includes(file.file_path);
            for (const auto& include_path : includes) {
                if (is_forbidden_include(include_path, boundary.forbidden_dependencies)) {
                    violations.push_back({
                        file.file_path,
                        boundary.name,
                        include_path,
                        boundary.reason
                    });
                }
            }
        }
    }

    return violations;
}

void DependencyValidator::print_violations(const std::vector<Violation>& violations) const {
    if (violations.empty()) {
        std::cerr << "TRACE: Dependency validation: PASS" << std::endl;
        return;
    }

    std::cerr << "TRACE: Dependency validation: FAIL" << std::endl;
    std::cerr << "TRACE: Dependency violations (" << violations.size() << "):" << std::endl;
    for (const auto& v : violations) {
        std::cerr << "TRACE:   File: " << v.source_file << std::endl;
        std::cerr << "TRACE:   Boundary: " << v.boundary_name << std::endl;
        std::cerr << "TRACE:   Forbidden include: " << v.forbidden_dependency << std::endl;
        std::cerr << "TRACE:   Reason: " << v.reason << std::endl;
        std::cerr << "TRACE:   ---" << std::endl;
    }
}
