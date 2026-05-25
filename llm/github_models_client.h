// github_models_client.h
// Minimal GitHub Models API client abstraction. Uses libcurl.

#pragma once

#include <string>

namespace LLM {
    struct Request {
        std::string prompt;
        int max_tokens = 512;
        double temperature = 0.0;
    };

    struct Response {
        std::string text;
        int status_code = 0;
        std::string raw;
    };

    class Client {
    public:
        Client();
        ~Client();
        Response complete(const Request& req);
    };
}
