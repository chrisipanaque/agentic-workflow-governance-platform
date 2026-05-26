#include "path_validator.hpp"
#include <iostream>

PathValidator::PathValidator(const std::vector<ConfigLoader::ForbiddenPath>& policies)
    : policies_(policies) {}

bool PathValidator::is_glob_pattern(const std::string& pattern) const {
    return pattern.find('*') != std::string::npos || 
           pattern.find('?') != std::string::npos;
}

bool PathValidator::simple_glob_match(const std::string& text, const std::string& pattern) const {
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

bool PathValidator::matches_pattern(const std::string& path, const std::string& pattern) const {
    if (!is_glob_pattern(pattern)) {
        // Exact match or substring match for literal patterns
        return path.find(pattern) != std::string::npos || path == pattern;
    }
    // Glob-style matching
    return simple_glob_match(path, pattern);
}

PathValidator::ValidationResult PathValidator::validate_diff(const DiffScanner::DiffStats& diff_stats) {
    ValidationResult result;
    result.is_valid = true;

    for (const auto& file : diff_stats.files) {
        for (const auto& policy : policies_) {
            if (matches_pattern(file.file_path, policy.path)) {
                result.is_valid = false;
                result.violations.push_back(
                    "Forbidden file changed: " + file.file_path + 
                    " (" + policy.reason + ")"
                );
            }
        }
    }

    return result;
}

void PathValidator::print_validation_result(const ValidationResult& result) const {
    if (result.is_valid) {
        std::cout << "Policy validation: PASS\n";
        return;
    }

    std::cout << "Policy validation: FAIL\n";
    std::cout << "Violations:\n";
    for (const auto& violation : result.violations) {
        std::cout << "  - " << violation << '\n';
    }
}
