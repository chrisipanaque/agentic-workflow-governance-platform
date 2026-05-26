#include "audit_log.hpp"
#include <fstream>
#include <chrono>
#include <filesystem>
#include <iomanip>
#include <sstream>
#include <iostream>

AuditLog::AuditLog(const std::string& output_dir)
    : output_dir_(output_dir) {
    ensure_directory_exists(output_dir);
}

void AuditLog::ensure_directory_exists(const std::string& dir) {
    std::filesystem::create_directories(dir);
}

std::string AuditLog::get_timestamp() const {
    auto now = std::chrono::system_clock::now();
    auto time_t_now = std::chrono::system_clock::to_time_t(now);
    auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(
        now.time_since_epoch()) % 1000;

    std::ostringstream oss;
    oss << std::put_time(std::localtime(&time_t_now), "%Y-%m-%dT%H:%M:%S");
    oss << "." << std::setfill('0') << std::setw(3) << ms.count();
    return oss.str();
}

std::string AuditLog::action_to_string(Action action) const {
    switch (action) {
        case Action::HEALTHCHECK: return "HEALTHCHECK";
        case Action::SHOW_POLICIES: return "SHOW_POLICIES";
        case Action::SCAN_DIFF: return "SCAN_DIFF";
        case Action::VALIDATE_POLICY: return "VALIDATE_POLICY";
        case Action::RISK_SCORE: return "RISK_SCORE";
        case Action::CHECK_APPROVAL: return "CHECK_APPROVAL";
        case Action::VALIDATE_ARCHITECTURE: return "VALIDATE_ARCHITECTURE";
    }
    return "UNKNOWN";
}

std::string AuditLog::status_to_string(Status status) const {
    switch (status) {
        case Status::SUCCESS: return "SUCCESS";
        case Status::FAILURE: return "FAILURE";
        case Status::WARNING: return "WARNING";
    }
    return "UNKNOWN";
}

void AuditLog::record_entry(Action action, Status status, const std::string& message,
                           const json& details) {
    AuditEntry entry{
        get_timestamp(),
        action,
        status,
        message,
        details
    };
    entries_.push_back(entry);
}

void AuditLog::write_report() {
    if (entries_.empty()) {
        return;
    }

    auto now = std::chrono::system_clock::now();
    auto time_t_now = std::chrono::system_clock::to_time_t(now);
    
    char filename[64];
    std::strftime(filename, sizeof(filename), "audit_%Y%m%d_%H%M%S.json", 
                  std::localtime(&time_t_now));

    std::string filepath = output_dir_ + "/" + filename;
    
    json report = json::array();
    for (const auto& entry : entries_) {
        json entry_json;
        entry_json["timestamp"] = entry.timestamp;
        entry_json["action"] = action_to_string(entry.action);
        entry_json["status"] = status_to_string(entry.status);
        entry_json["message"] = entry.message;
        entry_json["details"] = entry.details;
        report.push_back(entry_json);
    }

    std::ofstream file(filepath);
    if (!file.is_open()) {
        throw std::runtime_error("Failed to open audit file: " + filepath);
    }

    file << report.dump(2) << '\n';
    file.close();
}
