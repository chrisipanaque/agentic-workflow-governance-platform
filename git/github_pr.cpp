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

        // Determine remote origin URL
        std::string url = run_command_capture("git config --get remote.origin.url");
        if (url.empty()) {
            std::cerr << "[PR] could not read remote.origin.url" << std::endl;
            return false;
        }
        // Trim whitespace/newline
        while (!url.empty() && (url.back()=='\n' || url.back()=='\r' || url.back()==' ')) url.pop_back();

        // Support git@github.com:owner/repo.git and https forms
        std::string owner_repo;
        if (url.rfind("git@", 0) == 0) {
            // git@github.com:owner/repo.git
            auto pos = url.find(':');
            if (pos != std::string::npos) owner_repo = url.substr(pos+1);
        } else {
            // https://github.com/owner/repo.git
            auto pos = url.find("github.com/");
            if (pos != std::string::npos) owner_repo = url.substr(pos+11);
        }
        if (owner_repo.size() > 4 && owner_repo.substr(owner_repo.size()-4)==".git")
            owner_repo = owner_repo.substr(0, owner_repo.size()-4);

        if (owner_repo.empty()) {
            std::cerr << "[PR] could not parse owner/repo from origin url: " << url << std::endl;
            return false;
        }

        // Build API URL
        std::string api_url = "https://api.github.com/repos/" + owner_repo + "/pulls";

        // Minimal JSON body
        std::ostringstream payload;
        payload << "{\"title\":\"" << json_escape(title);
        payload << "\",\"head\":\"" << json_escape(branch_name);
        payload << "\",\"base\":\"main\",\"body\":\"" << json_escape(body) << "\"}";

        CURL* curl = curl_easy_init();
        if (!curl) return false;

        struct curl_slist* hdrs = nullptr;
        std::string auth = std::string("Authorization: token ") + token;
        hdrs = curl_slist_append(hdrs, "Accept: application/vnd.github.v3+json");
        hdrs = curl_slist_append(hdrs, auth.c_str());
        hdrs = curl_slist_append(hdrs, "User-Agent: planner-cli/1.0");
        hdrs = curl_slist_append(hdrs, "Content-Type: application/json");

        std::string resp_body;
        curl_easy_setopt(curl, CURLOPT_URL, api_url.c_str());
        curl_easy_setopt(curl, CURLOPT_POST, 1L);
        std::string pl = payload.str();
        curl_easy_setopt(curl, CURLOPT_POSTFIELDS, pl.c_str());
        curl_easy_setopt(curl, CURLOPT_HTTPHEADER, hdrs);
        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_cb);
        curl_easy_setopt(curl, CURLOPT_WRITEDATA, &resp_body);

        CURLcode res = curl_easy_perform(curl);
        bool ok = true;
        if (res != CURLE_OK) {
            std::cerr << "[PR] curl error: " << curl_easy_strerror(res) << std::endl;
            ok = false;
        } else {
            long code = 0;
            curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &code);
            if (code < 200 || code >= 300) {
                std::cerr << "[PR] GitHub API returned status " << code << ", body: " << resp_body << std::endl;
                ok = false;
            } else {
                std::cout << "[PR] pull request created successfully. Response: " << resp_body << std::endl;
            }
        }

        curl_slist_free_all(hdrs);
        curl_easy_cleanup(curl);
        return ok;
    }
}
