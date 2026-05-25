// branch_manager.h
// Create local git branches deterministically. Uses `git` command-line.

#pragma once

#include <string>

namespace Git {
    // Create a branch and check it out. Returns true on success.
    bool create_branch(const std::string& branch_name);
}
