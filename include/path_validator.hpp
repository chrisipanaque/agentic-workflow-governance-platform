#pragma once

#include <string>
#include <vector>
#include "config_loader.hpp"
#include "diff_scanner.hpp"

class PathValidator {
public:
    struct ValidationResult {
        bool is_valid = true;
        std::vector<std::string> violations;
    };

    explicit PathValidator(const std::vector<ConfigLoader::ForbiddenPath>& policies);

    ValidationResult validate_diff(const DiffScanner::DiffStats& diff_stats);
    void print_validation_result(const ValidationResult& result) const;

private:
    std::vector<ConfigLoader::ForbiddenPath> policies_;

    bool matches_pattern(const std::string& path, const std::string& pattern) const;
    bool is_glob_pattern(const std::string& pattern) const;
    bool simple_glob_match(const std::string& text, const std::string& pattern) const;
};
