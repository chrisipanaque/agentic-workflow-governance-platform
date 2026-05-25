// cpp_scanner.cpp
// Deterministic repository scanner implementation.
// - Recursively scans a root directory
// - Collects .cpp, .h, .hpp files
// - Skips configured ignore folders
// - Returns small snippets (first N lines) for context

#include "cpp_scanner.h"
#include <filesystem>
#include <fstream>
#include <algorithm>
#include <iostream>

namespace fs = std::filesystem;

namespace Repository {

    static const std::vector<std::string> kExtensions = {".cpp", ".cc", ".cxx", ".c", ".h", ".hpp"};
    static const std::vector<std::string> kIgnoreDirs = {"node_modules", "build", ".git", "dist", "vendor"};

    static bool has_extension(const fs::path& p) {
        std::string ext = p.extension().string();
        std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);
        for (auto &e : kExtensions) if (ext == e) return true;
        return false;
    }

    static bool is_ignored(const fs::path& p) {
        for (auto &d : kIgnoreDirs) {
            if (p.filename() == d) return true;
        }
        return false;
    }

    static std::string read_snippet(const fs::path& path, size_t max_lines = 40) {
        std::ifstream ifs(path);
        if (!ifs) return "";
        std::string line;
        std::string out;
        size_t count = 0;
        while (std::getline(ifs, line) && count < max_lines) {
            out += line + "\n";
            ++count;
        }
        return out;
    }

    ScanResult scan_cpp_files(const std::string& root) {
        ScanResult result;
        std::vector<fs::path> to_visit;
        try {
            fs::path r(root);
            if (!fs::exists(r)) return result;

            // Collect paths deterministic: iterate and sort
                for (auto& p : fs::recursive_directory_iterator(r)) {
                // skip directories matching ignore list
                bool skip = false;
                    for (auto const &part : p.path()) {
                        if (is_ignored(part)) { skip = true; break; }
                    }
                if (skip) continue;
                if (!fs::is_regular_file(p.path())) continue;
                if (!has_extension(p.path())) continue;
                to_visit.push_back(p.path());
            }

            std::sort(to_visit.begin(), to_visit.end());
            for (auto &fp : to_visit) {
                FileSnippet fsn;
                fsn.path = fp.lexically_normal().string();
                fsn.snippet = read_snippet(fp);
                result.files.push_back(std::move(fsn));
            }
        } catch (const std::exception& e) {
            // deterministic tooling: log to stderr but continue
            std::cerr << "[SCAN] exception: " << e.what() << std::endl;
        }
        return result;
    }
}
