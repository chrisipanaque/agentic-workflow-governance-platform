// markdown_generator.cpp
// Minimal markdown handling. Keeps content unchanged but enforces that the
// document starts with a header and is not empty. This is intentionally small
// since the LLM is responsible for generating the plan.

#include "markdown_generator.h"
#include <algorithm>
#include <sstream>

namespace Output {
    std::string sanitize_markdown(const std::string& raw) {
        std::string out = raw;
        // Trim leading/trailing whitespace
        auto l = out.find_first_not_of(" \n\t\r");
        if (l == std::string::npos) return "# Plan\n\n(LLM returned empty response)";
        out = out.substr(l);
        auto r = out.find_last_not_of(" \n\t\r");
        if (r != std::string::npos) out = out.substr(0, r+1);

        // If it doesn't start with a markdown header, prefix one
        if (out.size() >= 1 && out[0] != '#') {
            std::ostringstream ss;
            ss << "# Plan\n\n" << out;
            out = ss.str();
        }
        return out;
    }
}
