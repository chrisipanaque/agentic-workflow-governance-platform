// github_pr.h
// Create a GitHub pull request using the repository's remote origin.

#pragma once

#include <string>

namespace GitHubPR {
    // Create a PR with title and body from branch -> default branch.
    // Returns true on success.
    bool create_pull_request(const std::string& title, const std::string& body, const std::string& branch_name);
}
