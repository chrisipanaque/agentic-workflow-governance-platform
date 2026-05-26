#pragma once

#include <string>
#include <vector>
#include <nlohmann/json.hpp>

using json = nlohmann::json;

class TraceLogger {
public:
    explicit TraceLogger(const std::string& output_dir = "output/traces");

    void log_trace(const std::string& event_type, const json& data);
    void flush();

private:
    std::string output_dir_;
    std::vector<json> traces_;

    void ensure_directory_exists(const std::string& dir);
};
