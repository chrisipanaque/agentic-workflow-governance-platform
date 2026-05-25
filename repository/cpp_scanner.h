// cpp_scanner.h
// Deterministic scanner for C++ source files. Does not execute code.

#pragma once

#include <string>
#include <vector>

namespace Repository {
    struct FileSnippet {
        std::string path;     // repository-relative path
        std::string snippet;  // small sample of the file
    };

    struct ScanResult {
        std::vector<FileSnippet> files;
    };

    // Scan a directory recursively and collect .cpp, .h, .hpp files.
    // Deterministic: processes entries in sorted order.
    ScanResult scan_cpp_files(const std::string& root);
}
