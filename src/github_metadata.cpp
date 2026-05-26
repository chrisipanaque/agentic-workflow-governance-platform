#include "github_metadata.hpp"
#include <cstdlib>
#include <iostream>

GitHubMetadata::GitHubMetadata() {
    load_from_environment();
}

std::string GitHubMetadata::get_env(const std::string& key) const {
    const char* value = std::getenv(key.c_str());
    return value ? std::string(value) : "";
}

void GitHubMetadata::load_from_environment() {
    metadata_.pr_number = get_env("GITHUB_REF_NAME");
    metadata_.commit_sha = get_env("GITHUB_SHA");
    metadata_.actor = get_env("GITHUB_ACTOR");
    metadata_.event_name = get_env("GITHUB_EVENT_NAME");
    metadata_.repository = get_env("GITHUB_REPOSITORY");
    
    // Extract branch and PR info
    std::string github_ref = get_env("GITHUB_REF");
    metadata_.branch = github_ref;
    
    // Check if this is a pull request
    if (metadata_.event_name == "pull_request") {
        metadata_.is_pull_request = true;
        // Try to get PR number from event payload or environment
        std::string pr_num = get_env("GITHUB_EVENT_PULL_REQUEST_NUMBER");
        if (!pr_num.empty()) {
            metadata_.pr_number = pr_num;
        }
    }
    
    // Clean up branch refs/heads/branch-name -> branch-name
    if (metadata_.branch.substr(0, 11) == "refs/heads/") {
        metadata_.branch = metadata_.branch.substr(11);
    }
    if (metadata_.branch.substr(0, 10) == "refs/pull/") {
        // refs/pull/123/merge -> PR 123
        size_t slash_pos = metadata_.branch.find('/', 10);
        if (slash_pos != std::string::npos) {
            metadata_.pr_number = metadata_.branch.substr(10, slash_pos - 10);
        }
    }
}

GitHubMetadata::PRMetadata GitHubMetadata::get_pr_metadata() const {
    return metadata_;
}

json GitHubMetadata::to_json() const {
    json j;
    j["pr_number"] = metadata_.pr_number;
    j["branch"] = metadata_.branch;
    j["commit_sha"] = metadata_.commit_sha;
    j["actor"] = metadata_.actor;
    j["event_name"] = metadata_.event_name;
    j["repository"] = metadata_.repository;
    j["is_pull_request"] = metadata_.is_pull_request;
    return j;
}

void GitHubMetadata::print_summary() const {
    std::cout << "GitHub Context:\n";
    std::cout << "  Repository: " << metadata_.repository << '\n';
    std::cout << "  Event: " << metadata_.event_name << '\n';
    std::cout << "  Branch: " << metadata_.branch << '\n';
    if (!metadata_.pr_number.empty() && metadata_.is_pull_request) {
        std::cout << "  PR Number: " << metadata_.pr_number << '\n';
    }
    std::cout << "  Commit: " << metadata_.commit_sha.substr(0, 8) << '\n';
    std::cout << "  Actor: " << metadata_.actor << '\n';
}
