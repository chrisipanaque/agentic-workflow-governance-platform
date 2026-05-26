#include "diff_scanner.hpp"
#include <iostream>
#include <cstdlib>
#include <regex>
#include <sstream>
#include <memory>

DiffScanner::DiffScanner() {}

DiffScanner::FileDiff DiffScanner::parse_file_diff(const std::string& hunk_header) const {
    // Parse hunk header like: @@ -10,5 +10,7 @@ function_name
    // Extracts line counts for additions and deletions
    std::regex hunk_regex(R"(@@ -(\d+)(?:,(\d+))? \+(\d+)(?:,(\d+))? @@)");
    std::smatch match;

    FileDiff diff;
    if (std::regex_search(hunk_header, match, hunk_regex)) {
        // match[2] is deletion count (or 1 if not specified), match[4] is addition count
        diff.deletions = match[2].matched ? std::stoi(match[2]) : 1;
        diff.additions = match[4].matched ? std::stoi(match[4]) : 1;
    }
    return diff;
}

DiffScanner::DiffStats DiffScanner::scan() {
    DiffStats stats;
    FILE* pipe = popen("git diff --unified=0", "r");
    if (!pipe) {
        throw std::runtime_error("Failed to run git diff");
    }

    std::string current_file;
    char buffer[256];
    int current_additions = 0;
    int current_deletions = 0;
    bool in_diff = false;

    while (fgets(buffer, sizeof(buffer), pipe)) {
        std::string line(buffer);
        
        // New file diff header
        if (line.substr(0, 4) == "diff") {
            if (in_diff && !current_file.empty()) {
                stats.files.push_back({current_file, current_additions, current_deletions});
                stats.total_additions += current_additions;
                stats.total_deletions += current_deletions;
            }
            current_file.clear();
            current_additions = 0;
            current_deletions = 0;
            in_diff = true;
        }
        
        // Extract filename from +++ line
        if (line.substr(0, 4) == "+++ ") {
            current_file = line.substr(6); // Skip "+++ b/"
            if (!current_file.empty() && current_file.back() == '\n') {
                current_file.pop_back();
            }
        }
        
        // Parse hunk headers
        if (line.substr(0, 2) == "@@") {
            FileDiff hunk_diff = parse_file_diff(line);
            current_additions += hunk_diff.additions;
            current_deletions += hunk_diff.deletions;
        }
    }

    // Don't forget last file
    if (in_diff && !current_file.empty()) {
        stats.files.push_back({current_file, current_additions, current_deletions});
        stats.total_additions += current_additions;
        stats.total_deletions += current_deletions;
    }

    pclose(pipe);
    return stats;
}

void DiffScanner::print_stats(const DiffStats& stats) const {
    if (stats.files.empty()) {
        std::cout << "No changes detected\n";
        return;
    }

    std::cout << "Changed files: " << stats.files.size() << '\n';
    for (const auto& file : stats.files) {
        std::cout << "  " << file.file_path 
                  << " (++" << file.additions << " -" << file.deletions << ")\n";
    }
    std::cout << "Total: ++" << stats.total_additions 
              << " -" << stats.total_deletions << '\n';
}
