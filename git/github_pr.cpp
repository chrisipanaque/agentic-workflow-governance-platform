// github_pr.cpp
// Minimal PR creation using libcurl and the GitHub REST API.
// Expects environment variable GITHUB_TOKEN with repo permissions.

#include "github_pr.h"
#include <cstdlib>
#include <iostream>
#include <sstream>
#include <string>
#include <array>
#include <cstdio>
#include <curl/curl.h>
#include <sys/wait.h>

static std::string run_command_capture(const std::string& cmd) {
    std::array<char, 128> buffer;
    std::string result;
    FILE* pipe = popen(cmd.c_str(), "r");
    if (!pipe) return result;
    while (fgets(buffer.data(), buffer.size(), pipe) != nullptr) {
        result += buffer.data();
    }
    pclose(pipe);
    return result;
}

namespace GitHubPR {

    static std::string trim(const std::string& input) {
        size_t start = 0;
        while (start < input.size() && (input[start] == ' ' || input[start] == '\t' || input[start] == '\n' || input[start] == '\r')) {
            ++start;
        }
        size_t end = input.size();
        while (end > start && (input[end - 1] == ' ' || input[end - 1] == '\t' || input[end - 1] == '\n' || input[end - 1] == '\r')) {
            --end;
        }
        return input.substr(start, end - start);
    }

    static std::string json_escape(const std::string& input) {
        std::string out;
        out.reserve(input.size());
        for (unsigned char c : input) {
            switch (c) {
                case '"': out += "\\\""; break;
                case '\\': out += "\\\\"; break;
                case '\b': out += "\\b"; break;
                case '\f': out += "\\f"; break;
                case '\n': out += "\\n"; break;
                case '\r': out += "\\r"; break;
                case '\t': out += "\\t"; break;
                default:
                    if (c < 0x20) {
                        char buf[7];
                        snprintf(buf, sizeof(buf), "\\u%04x", c);
                        out += buf;
                    } else {
                        out += c;
                    }
            }
        }
        return out;
    }

    static bool is_valid_branch_name(const std::string& name, std::string& reason) {
        if (name.empty()) {
            reason = "branch name is empty";
            return false;
        }
        if (name == "refs/heads/" || name.rfind("refs/heads/", 0) == 0) {
            reason = "branch name must not include refs/heads/ prefix";
            return false;
        }
        if (name.find_first_of(" \t\n\r") != std::string::npos) {
            reason = "branch name contains whitespace";
            return false;
        }
        return true;
    }

    static bool branch_exists_locally(const std::string& branch_name) {
        std::string cmd = "git rev-parse --verify --quiet " + branch_name + " 2>/dev/null";
        std::string output = run_command_capture(cmd);
        return !trim(output).empty();
    }

    static bool branch_exists_remotely(const std::string& branch_name) {
        std::string cmd = "git ls-remote --heads origin " + branch_name + " 2>/dev/null";
        std::string output = run_command_capture(cmd);
        return !trim(output).empty();
    }

    static bool push_branch_to_origin(const std::string& branch_name, std::string& reason) {
        std::cout << "[PR] pushing branch to origin: " << branch_name << std::endl;
        std::string cmd = "git push -u origin " + branch_name;
        int result = std::system(cmd.c_str());
        if (result != 0) {
            reason = "git push failed with exit code " + std::to_string(result);
            return false;
        }
        return true;
    }

    static std::string get_default_base_branch() {
        std::string output = run_command_capture("git remote show origin 2>/dev/null | grep 'HEAD branch' | sed 's/.*: //' 2>/dev/null");
        std::string branch = trim(output);
        if (!branch.empty()) {
            return branch;
        }
        return "main";
    }

    static bool checkout_branch(const std::string& branch_name, std::string& reason) {
        std::ostringstream cmd;
        cmd << "git checkout " << branch_name << " 2>/dev/null";
        int result = std::system(cmd.str().c_str());
        if (result != 0) {
            reason = "git checkout " + branch_name + " failed with exit code " + std::to_string(result);
            return false;
        }
        return true;
    }

    static bool current_branch_is(const std::string& branch_name) {
        std::string current_branch = trim(run_command_capture("git rev-parse --abbrev-ref HEAD 2>/dev/null"));
        return current_branch == branch_name;
    }

    static void delete_local_branch(const std::string& branch_name, const std::string& fallback_branch) {
        std::string current_branch = trim(run_command_capture("git rev-parse --abbrev-ref HEAD 2>/dev/null"));
        if (current_branch == branch_name) {
            std::cout << "[PR] currently on branch '" << branch_name << "', switching to '" << fallback_branch << "' before deletion." << std::endl;
            std::string checkout_error;
            if (!checkout_branch(fallback_branch, checkout_error)) {
                std::cerr << "[PR] could not switch off branch before deletion: " << checkout_error << std::endl;
                std::cerr << "[PR] branch '" << branch_name << "' will not be deleted automatically." << std::endl;
                return;
            }
        }
        std::cout << "[PR] cleaning up local branch: " << branch_name << std::endl;
        std::string cmd = "git branch -D " + branch_name + " 2>/dev/null";
        std::system(cmd.c_str());
    }

    bool validatePullRequestBranches(const std::string& branch_name, const std::string& base_branch, std::string& error) {
        if (!is_valid_branch_name(branch_name, error)) {
            return false;
        }
        if (!branch_exists_locally(branch_name)) {
            error = "local branch '" + branch_name + "' does not exist";
            return false;
        }
        if (!branch_exists_remotely(branch_name)) {
            if (!push_branch_to_origin(branch_name, error)) {
                return false;
            }
            if (!branch_exists_remotely(branch_name)) {
                error = "branch '" + branch_name + "' still does not exist on origin after push";
                return false;
            }
        }
        if (!branch_has_commits_ahead(branch_name, base_branch)) {
            error = "branch '" + branch_name + "' has no commits ahead of base branch '" + base_branch + "'";
            return false;
        }
        return true;
    }

    static size_t write_cb(void* ptr, size_t size, size_t nmemb, void* userdata) {
        std::string* s = static_cast<std::string*>(userdata);
        s->append(static_cast<char*>(ptr), size * nmemb);
        return size * nmemb;
    }

    bool create_pull_request(const std::string& title, const std::string& body, const std::string& branch_name) {
        const char* token = std::getenv("GITHUB_TOKEN");
        if (!token) {
            std::cerr << "[PR] missing GITHUB_TOKEN environment variable" << std::endl;
            return false;
        }

        if (branch_name.empty()) {
            std::cerr << "[PR] head branch name is empty" << std::endl;
            return false;
        }

        std::string remote_url = run_command_capture("git config --get remote.origin.url");
        remote_url = trim(remote_url);
        if (remote_url.empty()) {
            std::cerr << "[PR] could not read remote.origin.url" << std::endl;
            return false;
        }

        std::string owner_repo;
        if (remote_url.rfind("git@", 0) == 0) {
            auto pos = remote_url.find(':');
            if (pos != std::string::npos) owner_repo = remote_url.substr(pos + 1);
        } else {
            auto pos = remote_url.find("github.com/");
            if (pos != std::string::npos) owner_repo = remote_url.substr(pos + 11);
        }
        if (owner_repo.size() > 4 && owner_repo.substr(owner_repo.size() - 4) == ".git") {
            owner_repo = owner_repo.substr(0, owner_repo.size() - 4);
        }

        if (owner_repo.empty()) {
            std::cerr << "[PR] could not parse owner/repo from origin url: " << remote_url << std::endl;
            return false;
        }

        std::string base_branch = get_default_base_branch();
        std::cout << "[PR] remote origin=" << remote_url << " owner/repo=" << owner_repo << std::endl;
        std::cout << "[PR] validating branch '" << branch_name << "' against base '" << base_branch << "'" << std::endl;

        std::string validate_error;
        if (!validatePullRequestBranches(branch_name, base_branch, validate_error)) {
            std::cerr << "[PR] validation failed: " << validate_error << std::endl;
            delete_local_branch(branch_name, base_branch);
            return false;
        }

        std::string api_url = "https://api.github.com/repos/" + owner_repo + "/pulls";
        std::ostringstream payload;
        payload << "{\"title\":\"" << json_escape(title);
        payload << "\",\"head\":\"" << json_escape(branch_name);
        payload << "\",\"base\":\"" << json_escape(base_branch) << "\",\"body\":\"" << json_escape(body) << "\"}";
        std::string payload_str = payload.str();

        std::cout << "[PR] payload=" << payload_str << std::endl;

        auto send_request = [&](std::string& response_body, long& response_code) -> bool {
            CURL* curl = curl_easy_init();
            if (!curl) return false;
            struct curl_slist* hdrs = nullptr;
            std::string auth = std::string("Authorization: token ") + token;
            hdrs = curl_slist_append(hdrs, "Accept: application/vnd.github.v3+json");
            hdrs = curl_slist_append(hdrs, auth.c_str());
            hdrs = curl_slist_append(hdrs, "User-Agent: planner-cli/1.0");
            hdrs = curl_slist_append(hdrs, "Content-Type: application/json");

            curl_easy_setopt(curl, CURLOPT_URL, api_url.c_str());
            curl_easy_setopt(curl, CURLOPT_POST, 1L);
            curl_easy_setopt(curl, CURLOPT_POSTFIELDS, payload_str.c_str());
            curl_easy_setopt(curl, CURLOPT_HTTPHEADER, hdrs);
            curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_cb);
            curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response_body);

            CURLcode res = curl_easy_perform(curl);
            if (res != CURLE_OK) {
                std::cerr << "[PR] curl error: " << curl_easy_strerror(res) << std::endl;
                curl_slist_free_all(hdrs);
                curl_easy_cleanup(curl);
                return false;
            }
            curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &response_code);
            curl_slist_free_all(hdrs);
            curl_easy_cleanup(curl);
            return true;
        };

        std::string response_body;
        long response_code = 0;
        bool success = send_request(response_body, response_code);
        if (!success) {
            std::cerr << "[PR] failed to send request" << std::endl;
            delete_local_branch(branch_name, base_branch);
            return false;
        }

        if (response_code >= 200 && response_code < 300) {
            std::cout << "[PR] pull request created successfully. Response: " << response_body << std::endl;
            return true;
        }

        std::cerr << "[PR] GitHub API returned status " << response_code << ", body: " << response_body << std::endl;
        if (response_code == 422) {
            std::cerr << "[PR] validation error: head branch may not exist on remote or may be malformed." << std::endl;
            std::cerr << "[PR] retrying validation and making a second attempt." << std::endl;
            std::string retry_error;
            if (!validatePullRequestBranches(branch_name, base_branch, retry_error)) {
                std::cerr << "[PR] retry validation failed: " << retry_error << std::endl;
                delete_local_branch(branch_name, base_branch);
                return false;
            }
            response_body.clear();
            if (!send_request(response_body, response_code)) {
                std::cerr << "[PR] retry failed to send request" << std::endl;
                delete_local_branch(branch_name, base_branch);
                return false;
            }
            if (response_code >= 200 && response_code < 300) {
                std::cout << "[PR] pull request created successfully on retry. Response: " << response_body << std::endl;
                return true;
            }
            std::cerr << "[PR] retry returned status " << response_code << ", body: " << response_body << std::endl;
        }

        delete_local_branch(branch_name, base_branch);
        return false;
    }
}
