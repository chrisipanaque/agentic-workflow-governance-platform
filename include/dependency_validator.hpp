#pragma once

#include <string>
#include <vector>
#include "diff_scanner.hpp"

class DependencyValidator {
public:
    struct Boundary {
        std::string name;
        std::string source_pattern;
        std::vector<std::string> forbidden_dependencies;
        std::string reason;
    };

    struct Violation {
        std::string source_file;
        std::string boundary_name;
        std::string forbidden_dependency;
        std::string reason;
    };

    explicit DependencyValidator(const std::string& config_path);

    void load_rules();
    std::vector<Violation> validate(const DiffScanner::DiffStats& diff_stats);
    void print_violations(const std::vector<Violation>& violations) const;

private:
    std::string config_path_;
    std::vector<Boundary> boundaries_;

    bool path_matches_pattern(const std::string& path, const std::string& pattern) const;
    bool simple_glob_match(const std::string& text, const std::string& pattern) const;
    bool is_glob_pattern(const std::string& pattern) const;
    std::vector<std::string> extract_includes(const std::string& file_path) const;
    bool is_forbidden_include(const std::string& include_path, const std::vector<std::string>& forbidden) const;
};
