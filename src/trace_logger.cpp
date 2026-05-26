#include "trace_logger.hpp"
#include <fstream>
#include <chrono>
#include <filesystem>
#include <sstream>
#include <iostream>

TraceLogger::TraceLogger(const std::string& output_dir)
    : output_dir_(output_dir) {
    ensure_directory_exists(output_dir);
}

void TraceLogger::ensure_directory_exists(const std::string& dir) {
    std::filesystem::create_directories(dir);
}

void TraceLogger::log_trace(const std::string& event_type, const json& data) {
    auto now = std::chrono::system_clock::now();
    auto time_t_now = std::chrono::system_clock::to_time_t(now);
    auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(
        now.time_since_epoch()) % 1000;

    char timestamp[64];
    std::strftime(timestamp, sizeof(timestamp), "%Y-%m-%dT%H:%M:%S", 
                  std::localtime(&time_t_now));

    json trace;
    trace["timestamp"] = std::string(timestamp) + "." + 
                        (ms.count() < 100 ? "0" : "") + 
                        std::to_string(ms.count());
    trace["event_type"] = event_type;
    trace["data"] = data;

    traces_.push_back(trace);
}

void TraceLogger::flush() {
    if (traces_.empty()) {
        return;
    }

    auto now = std::chrono::system_clock::now();
    auto time_t_now = std::chrono::system_clock::to_time_t(now);
    
    char filename[64];
    std::strftime(filename, sizeof(filename), "trace_%Y%m%d_%H%M%S.jsonl", 
                  std::localtime(&time_t_now));

    std::string filepath = output_dir_ + "/" + filename;
    std::ofstream file(filepath, std::ios::app);
    if (!file.is_open()) {
        throw std::runtime_error("Failed to open trace file: " + filepath);
    }

    for (const auto& trace : traces_) {
        file << trace.dump() << '\n';
    }

    file.close();
    traces_.clear();
}
