// env_loader.cpp
// Simple .env loader for local development. This allows the tool to honor
// environment variables defined in a .env file in the repository root.

#include "env_loader.h"
#include <fstream>
#include <cstdlib>
#include <sstream>
#include <algorithm>
#include <cctype>

namespace Env {

    static inline std::string trim(const std::string& input) {
        size_t start = 0;
        while (start < input.size() && std::isspace(static_cast<unsigned char>(input[start]))) {
            ++start;
        }
        size_t end = input.size();
        while (end > start && std::isspace(static_cast<unsigned char>(input[end - 1]))) {
            --end;
        }
        return input.substr(start, end - start);
    }

    bool load_dotenv(const std::string& path) {
        std::ifstream ifs(path);
        if (!ifs.is_open()) {
            return false;
        }

        std::string line;
        while (std::getline(ifs, line)) {
            std::string trimmed = trim(line);
            if (trimmed.empty() || trimmed[0] == '#') {
                continue;
            }

            size_t eq = trimmed.find('=');
            if (eq == std::string::npos) {
                continue;
            }

            std::string key = trim(trimmed.substr(0, eq));
            std::string value = trim(trimmed.substr(eq + 1));

            if (key.empty()) {
                continue;
            }

            if (std::getenv(key.c_str()) == nullptr) {
                setenv(key.c_str(), value.c_str(), 1);
            }
        }

        return true;
    }
}
