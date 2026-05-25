// cli_parser.cpp
// Minimal CLI parsing. Keeps parsing deterministic and simple.

#include "cli_parser.h"
#include <iostream>

namespace CLI {
    std::string parse(int argc, char** argv) {
        // Expect a single argument: the task description.
        if (argc < 2) {
            return "";
        }

        // Concatenate all args into a single task string (supports quotes).
        std::string task;
        for (int i = 1; i < argc; ++i) {
            if (i > 1) task += " ";
            task += argv[i];
        }
        return task;
    }
}
