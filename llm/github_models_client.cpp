// github_models_client.cpp
// Very small HTTP client to call GitHub Models API (or configurable endpoint).
// This implementation uses libcurl and expects an API key in environment
// variable GITHUB_MODELS_API_KEY and an optional endpoint in GITHUB_MODELS_API_URL.

#include "github_models_client.h"
#include <curl/curl.h>
#include <cstdlib>
#include <iostream>
#include <sstream>

namespace LLM
{

    static std::string json_escape(const std::string &input)
    {
        std::string out;
        out.reserve(input.size());
        for (unsigned char c : input)
        {
            switch (c)
            {
            case '"':
                out += "\\\"";
                break;
            case '\\':
                out += "\\\\";
                break;
            case '\b':
                out += "\\b";
                break;
            case '\f':
                out += "\\f";
                break;
            case '\n':
                out += "\\n";
                break;
            case '\r':
                out += "\\r";
                break;
            case '\t':
                out += "\\t";
                break;
            default:
                if (c < 0x20)
                {
                    char buf[7];
                    snprintf(buf, sizeof(buf), "\\u%04x", c);
                    out += buf;
                }
                else
                {
                    out += c;
                }
            }
        }
        return out;
    }

    static size_t write_cb(void *ptr, size_t size, size_t nmemb, void *userdata)
    {
        std::string *s = static_cast<std::string *>(userdata);
        s->append(static_cast<char *>(ptr), size * nmemb);
        return size * nmemb;
    }

    Client::Client() {}
    Client::~Client() {}

    Response Client::complete(const Request &req)
    {
        Response out;
        const char *api_key = std::getenv("GITHUB_MODELS_API_KEY");
        const char *api_url = std::getenv("GITHUB_MODELS_API_URL");
        const char *api_provider = std::getenv("GITHUB_MODELS_PROVIDER");
        if (!api_key)
        {
            std::cerr << "[LLM] missing GITHUB_MODELS_API_KEY environment variable" << std::endl;
            out.status_code = 0;
            return out;
        }
        std::string url = api_url ? api_url : "https://models.github.ai/inference/chat/completions";

        CURL *curl = curl_easy_init();
        if (!curl)
        {
            std::cerr << "[LLM] curl init failed" << std::endl;
            return out;
        }

        // Build a chat completions JSON payload. Convert prompt to messages format.
        std::ostringstream payload;
        payload << "{\"model\":\"" << json_escape(req.model) << "\",\"messages\":[{\"role\":\"user\",\"content\":\"" << json_escape(req.prompt) << "\"}],\"max_tokens\":" << req.max_tokens << ",\"temperature\":" << req.temperature;
        if (api_provider)
        {
            payload << ",\"provider\":\"" << json_escape(api_provider) << "\"";
        }
        payload << "}";

        std::string response_data;
        struct curl_slist *headers = nullptr;
        std::string auth = "Authorization: Bearer ";
        auth += api_key;
        headers = curl_slist_append(headers, "Accept: application/vnd.github+json");
        headers = curl_slist_append(headers, "Content-Type: application/json");
        headers = curl_slist_append(headers, auth.c_str());
        headers = curl_slist_append(headers, "User-Agent: planner-cli/1.0");

        curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
        curl_easy_setopt(curl, CURLOPT_POST, 1L);
        std::string payload_str = payload.str();
        curl_easy_setopt(curl, CURLOPT_POSTFIELDS, payload_str.c_str());
        curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_cb);
        curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response_data);

        CURLcode res = curl_easy_perform(curl);
        if (res != CURLE_OK)
        {
            std::cerr << "[LLM] curl perform error: " << curl_easy_strerror(res) << std::endl;
        }
        else
        {
            long code = 0;
            curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &code);
            out.status_code = static_cast<int>(code);
            out.raw = response_data;
            
            // Extract content from chat completions response format.
            // Response format: {"choices":[{"message":{"content":"..."}}]}
            std::string content_key = "\"content\":\"";
            size_t pos = response_data.find(content_key);
            if (pos != std::string::npos)
            {
                size_t start = pos + content_key.length();
                size_t end = response_data.find("\"", start);
                if (end != std::string::npos)
                {
                    out.text = response_data.substr(start, end - start);
                    // Unescape JSON escape sequences in the content
                    std::string unescaped;
                    for (size_t i = 0; i < out.text.length(); ++i)
                    {
                        if (out.text[i] == '\\' && i + 1 < out.text.length())
                        {
                            char next = out.text[i + 1];
                            if (next == 'n')
                            {
                                unescaped += '\n';
                                ++i;
                            }
                            else if (next == 't')
                            {
                                unescaped += '\t';
                                ++i;
                            }
                            else if (next == 'r')
                            {
                                unescaped += '\r';
                                ++i;
                            }
                            else if (next == '\\')
                            {
                                unescaped += '\\';
                                ++i;
                            }
                            else if (next == '"')
                            {
                                unescaped += '"';
                                ++i;
                            }
                            else
                            {
                                unescaped += out.text[i];
                            }
                        }
                        else
                        {
                            unescaped += out.text[i];
                        }
                    }
                    out.text = unescaped;
                }
                else
                {
                    out.text = response_data;
                }
            }
            else
            {
                // Fallback to raw response if format is unexpected
                out.text = response_data;
            }
        }

        curl_slist_free_all(headers);
        curl_easy_cleanup(curl);
        return out;
    }

}
