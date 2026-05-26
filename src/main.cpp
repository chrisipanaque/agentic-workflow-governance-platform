#include <iostream>
#include <string_view>
#include "config_loader.hpp"
#include "diff_scanner.hpp"
#include "path_validator.hpp"
#include "risk_engine.hpp"
#include "trace_logger.hpp"
#include "audit_log.hpp"
#include "github_metadata.hpp"

int main(int argc, char* argv[]) {
    if (argc < 2) {
        std::cerr << "Usage: ai-control-plane <command>\n";
        return 1;
    }

    std::string_view command{argv[1]};
    GitHubMetadata github_metadata;
    TraceLogger trace_logger;
    AuditLog audit_log;
    auto pr_meta = github_metadata.get_pr_metadata();

    if (command == "healthcheck") {
        try {
            std::cout << "AI control plane operational\n";
            json hc_details = github_metadata.to_json();
            hc_details["status"] = "operational";
            audit_log.record_entry(AuditLog::Action::HEALTHCHECK, AuditLog::Status::SUCCESS, 
                                  "Healthcheck passed", hc_details);
            audit_log.write_report();
            trace_logger.flush();
            return 0;
        } catch (const std::exception& e) {
            std::cerr << "Error: " << e.what() << '\n';
            audit_log.record_entry(AuditLog::Action::HEALTHCHECK, AuditLog::Status::FAILURE,
                                  std::string(e.what()));
            audit_log.write_report();
            return 1;
        }
    }

    if (command == "show-policies") {
        try {
            ConfigLoader loader("config/forbidden-paths.json");
            loader.load();

            const auto& paths = loader.get_forbidden_paths();
            std::cout << "Forbidden paths: " << paths.size() << '\n';
            for (const auto& policy : paths) {
                std::cout << "  - " << policy.path << " (" << policy.reason << ")\n";
            }
            
            json details;
            details["policy_count"] = paths.size();
                        for (const auto& meta : github_metadata.to_json().items()) {
                            details[meta.key()] = meta.value();
                        }
            trace_logger.log_trace("show_policies", details);
            audit_log.record_entry(AuditLog::Action::SHOW_POLICIES, AuditLog::Status::SUCCESS,
                                  "Loaded " + std::to_string(paths.size()) + " policies", details);
            audit_log.write_report();
            trace_logger.flush();
            return 0;
        } catch (const std::exception& e) {
            std::cerr << "Error: " << e.what() << '\n';
            audit_log.record_entry(AuditLog::Action::SHOW_POLICIES, AuditLog::Status::FAILURE,
                                  std::string(e.what()));
            audit_log.write_report();
            return 1;
        }
    }

    if (command == "scan-diff") {
        try {
            DiffScanner scanner;
            auto stats = scanner.scan();
            scanner.print_stats(stats);
            
            json details;
            details["files_changed"] = stats.files.size();
            details["total_additions"] = stats.total_additions;
            details["total_deletions"] = stats.total_deletions;
                        for (const auto& meta : github_metadata.to_json().items()) {
                            details[meta.key()] = meta.value();
                        }
            trace_logger.log_trace("scan_diff", details);
            audit_log.record_entry(AuditLog::Action::SCAN_DIFF, AuditLog::Status::SUCCESS,
                                  "Scanned " + std::to_string(stats.files.size()) + " changed files", details);
            audit_log.write_report();
            trace_logger.flush();
            return 0;
        } catch (const std::exception& e) {
            std::cerr << "Error: " << e.what() << '\n';
            audit_log.record_entry(AuditLog::Action::SCAN_DIFF, AuditLog::Status::FAILURE,
                                  std::string(e.what()));
            audit_log.write_report();
            return 1;
        }
    }

    if (command == "validate-policy") {
        try {
            ConfigLoader config_loader("config/forbidden-paths.json");
            config_loader.load();
            const auto& policies = config_loader.get_forbidden_paths();

            DiffScanner diff_scanner;
            auto diff_stats = diff_scanner.scan();

            PathValidator validator(policies);
            auto validation = validator.validate_diff(diff_stats);
            validator.print_validation_result(validation);
            
            json details;
            details["valid"] = validation.is_valid;
            details["violations"] = validation.violations.size();
                        for (const auto& meta : github_metadata.to_json().items()) {
                            details[meta.key()] = meta.value();
                        }
            trace_logger.log_trace("validate_policy", details);
            
            AuditLog::Status status = validation.is_valid ? AuditLog::Status::SUCCESS : AuditLog::Status::FAILURE;
            audit_log.record_entry(AuditLog::Action::VALIDATE_POLICY, status,
                                  validation.is_valid ? "Policy validation passed" : "Policy violations detected", details);
            audit_log.write_report();
            trace_logger.flush();

            return validation.is_valid ? 0 : 1;
        } catch (const std::exception& e) {
            std::cerr << "Error: " << e.what() << '\n';
            audit_log.record_entry(AuditLog::Action::VALIDATE_POLICY, AuditLog::Status::FAILURE,
                                  std::string(e.what()));
            audit_log.write_report();
            return 1;
        }
    }

    if (command == "risk-score") {
        try {
            DiffScanner diff_scanner;
            auto diff_stats = diff_scanner.scan();

            RiskEngine engine("config/risk-rules.json");
            engine.load_rules();
            auto risk = engine.calculate_score(diff_stats);
            engine.print_score(risk);
            
            json details;
            details["score"] = risk.score;
            details["severity"] = risk.severity == RiskEngine::Severity::CRITICAL ? "CRITICAL" :
                                   risk.severity == RiskEngine::Severity::HIGH ? "HIGH" :
                                   risk.severity == RiskEngine::Severity::MEDIUM ? "MEDIUM" : "LOW";
            details["recommendation"] = risk.recommendation;
            details["risk_factors"] = risk.risk_factors;
                        for (const auto& meta : github_metadata.to_json().items()) {
                            details[meta.key()] = meta.value();
                        }
            trace_logger.log_trace("risk_score", details);
            
            AuditLog::Status status = (risk.score >= 75.0) ? AuditLog::Status::FAILURE : AuditLog::Status::SUCCESS;
            audit_log.record_entry(AuditLog::Action::RISK_SCORE, status,
                                  "Risk score: " + std::to_string(static_cast<int>(risk.score)), details);
            audit_log.write_report();
            trace_logger.flush();

            return (risk.score >= 75.0) ? 1 : 0;
        } catch (const std::exception& e) {
            std::cerr << "Error: " << e.what() << '\n';
            audit_log.record_entry(AuditLog::Action::RISK_SCORE, AuditLog::Status::FAILURE,
                                  std::string(e.what()));
            audit_log.write_report();
            return 1;
        }
    }

    std::cerr << "Unknown command: " << command << '\n';
    return 1;
}
