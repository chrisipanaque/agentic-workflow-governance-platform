#pragma once

#include <string>
#include <vector>
#include <nlohmann/json.hpp>

using json = nlohmann::json;

class AuditLog {
public:
    enum class Action {
        HEALTHCHECK,
        SHOW_POLICIES,
        SCAN_DIFF,
        VALIDATE_POLICY,
        RISK_SCORE,
        CHECK_APPROVAL
    };

    enum class Status {
        SUCCESS,
        FAILURE,
        WARNING
    };

    struct AuditEntry {
        std::string timestamp;
        Action action;
        Status status;
        std::string message;
        json details;
    };

    explicit AuditLog(const std::string& output_dir = "output/reports");

    void record_entry(Action action, Status status, const std::string& message, 
                     const json& details = json::object());
    void write_report();

private:
    std::string output_dir_;
    std::vector<AuditEntry> entries_;

    void ensure_directory_exists(const std::string& dir);
    std::string action_to_string(Action action) const;
    std::string status_to_string(Status status) const;
    std::string get_timestamp() const;
};
