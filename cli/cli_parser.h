// cli_parser.h
// Simple CLI parser for planner-cli.
// Responsibility: extract the task string from argv.

#pragma once

#include <string>

namespace CLI {
    // Parse CLI args. Returns empty string on failure.
    std::string parse(int argc, char** argv);
}
