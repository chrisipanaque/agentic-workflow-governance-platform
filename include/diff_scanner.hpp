#pragma once

#include <string>
#include <vector>

class DiffScanner {
public:
    struct FileDiff {
        std::string file_path;
        int additions = 0;
        int deletions = 0;
    };

    struct DiffStats {
        std::vector<FileDiff> files;
        int total_additions = 0;
        int total_deletions = 0;
    };

    DiffScanner();

    DiffStats scan();
    void print_stats(const DiffStats& stats) const;

private:
    FileDiff parse_file_diff(const std::string& hunk_header) const;
};
