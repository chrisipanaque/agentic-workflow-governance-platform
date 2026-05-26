#pragma once

#include <string>
#include <vector>
#include "diff_scanner.hpp"

class OwnershipValidator {
public:
    struct Boundary {
        std::string name;
        std::string team;
        std::vector<std::string> directories;
    };

    struct OwnershipInfo {
        std::string file;
        std::string boundary_name;
        std::string team;
    };

    explicit OwnershipValidator(const std::string& config_path);

    void load_rules();
    std::vector<OwnershipInfo> get_file_ownership(const DiffScanner::DiffStats& diff_stats);
    std::vector<std::string> detect_cross_boundary(const DiffScanner::DiffStats& diff_stats);
    void print_ownership(const std::vector<OwnershipInfo>& ownership_info) const;
    void print_cross_boundary(const std::vector<std::string>& touched_boundaries) const;

private:
    std::string config_path_;
    std::vector<Boundary> boundaries_;

    bool path_matches_directory(const std::string& path, const std::string& directory) const;
};
