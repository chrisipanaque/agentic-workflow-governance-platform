#pragma once

#include <string>
#include <nlohmann/json.hpp>

using json = nlohmann::json;

class GitHubMetadata {
public:
    struct PRMetadata {
        std::string pr_number;
        std::string branch;
        std::string commit_sha;
        std::string actor;
        std::string event_name;
        std::string repository;
        bool is_pull_request = false;
    };

    GitHubMetadata();

    PRMetadata get_pr_metadata() const;
    json to_json() const;
    void print_summary() const;

private:
    PRMetadata metadata_;

    std::string get_env(const std::string& key) const;
    void load_from_environment();
};
