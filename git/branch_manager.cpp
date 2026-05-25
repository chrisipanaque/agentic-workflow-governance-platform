// branch_manager.cpp
// Minimal git branch creation using system calls. Deterministic, simple,
// and explicitly uses the user's git installation. This keeps permissions
// constrained: the tool only creates branches locally.

#include "branch_manager.h"
#include <cstdlib>
#include <string>
#include <iostream>

namespace Git {
    bool create_branch(const std::string& branch_name) {
        // Use git to create and checkout a new branch. This is a deterministic
        // operation and intentionally minimal. We capture the return code.
        std::string cmd = "git checkout -b " + branch_name;
        int rc = std::system(cmd.c_str());
        if (rc != 0) {
            std::cerr << "[GIT] git command failed with code " << rc << std::endl;
            return false;
        }
        return true;
    }
}
