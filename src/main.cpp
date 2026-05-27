#include <iostream>
#include <string_view>
#include "config_loader.hpp"
#include "diff_scanner.hpp"
#include "path_validator.hpp"
#include "trace_logger.hpp"
#include "audit_log.hpp"
#include "github_metadata.hpp"
#include "dependency_validator.hpp"

int main(int argc, char* argv[]) {
    std::cerr << "TRACE: main entered" << std::endl;
    if (argc < 2) {
        std::cerr << "Usage: agentic-workflow-governance-tools <command>\n";
        return 1;
    }

    std::string_view command{argv[1]};
    std::cerr << "TRACE: command=" << command << std::endl;
    GitHubMetadata github_metadata;
    std::cerr << "TRACE: GitHubMetadata constructed" << std::endl;
    TraceLogger trace_logger;
    std::cerr << "TRACE: TraceLogger constructed" << std::endl;
    AuditLog audit_log;
    std::cerr << "TRACE: AuditLog constructed" << std::endl;

    if (command == "healthcheck") {
        try {
            std::cerr << "TRACE: healthcheck start" << std::endl;
            std::cerr << "AI governance platform operational oh yeah\n";
            json hc_details = github_metadata.to_json();
            hc_details["status"] = "operational";
            audit_log.record_entry(AuditLog::Action::HEALTHCHECK, AuditLog::Status::SUCCESS, 
                                  "Healthcheck passed", hc_details);
            std::cerr << "TRACE: healthcheck audit recorded" << std::endl;
            audit_log.write_report();
            std::cerr << "TRACE: healthcheck report written" << std::endl;
            trace_logger.flush();
            std::cerr << "TRACE: healthcheck flush done" << std::endl;
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
            std::cerr << "TRACE: show-policies start" << std::endl;
            ConfigLoader loader("config/forbidden-paths.json");
            loader.load();
            std::cerr << "TRACE: config loaded" << std::endl;

            const auto& paths = loader.get_forbidden_paths();
            std::cerr << "Forbidden paths: " << paths.size() << '\n';
            std::cerr << "TRACE: about to iterate paths" << std::endl;
            for (const auto& policy : paths) {
                std::cerr << "  - " << policy.path << " (" << policy.reason << ")\n";
            }
            std::cerr << "TRACE: path iteration done" << std::endl;
            
            json details;
            details["policy_count"] = paths.size();
            std::cerr << "TRACE: building json details" << std::endl;
                        auto meta_json = github_metadata.to_json();
                        for (const auto& meta : meta_json.items()) {
                            details[meta.key()] = meta.value();
                        }
            std::cerr << "TRACE: json details built" << std::endl;
            trace_logger.log_trace("show_policies", details);
            std::cerr << "TRACE: trace logged" << std::endl;
            audit_log.record_entry(AuditLog::Action::SHOW_POLICIES, AuditLog::Status::SUCCESS,
                                  "Loaded " + std::to_string(paths.size()) + " policies", details);
            std::cerr << "TRACE: audit recorded" << std::endl;
            audit_log.write_report();
            std::cerr << "TRACE: report written" << std::endl;
            trace_logger.flush();
            std::cerr << "TRACE: flush done" << std::endl;

            std::cerr << "TRACE: show-policies returning 0" << std::endl;
            return 0;
        } catch (const std::exception& e) {
            std::cerr << "Error: " << e.what() << '\n';
            audit_log.record_entry(AuditLog::Action::SHOW_POLICIES, AuditLog::Status::FAILURE,
                                  std::string(e.what()));
            audit_log.write_report();
            std::cerr << "TRACE: show-policies returning 1 (exception)" << std::endl;
            return 1;
        }
    }

    if (command == "scan-diff") {
        try {
            std::cerr << "TRACE: scan-diff start" << std::endl;
            DiffScanner scanner;
            auto stats = scanner.scan();
            std::cerr << "TRACE: scan-diff scan complete" << std::endl;
            scanner.print_stats(stats);
            std::cerr << "TRACE: scan-diff stats printed" << std::endl;
            
            json details;
            details["files_changed"] = stats.files.size();
            details["total_additions"] = stats.total_additions;
            details["total_deletions"] = stats.total_deletions;
                        auto meta_json = github_metadata.to_json();
                        for (const auto& meta : meta_json.items()) {
                            details[meta.key()] = meta.value();
                        }
            trace_logger.log_trace("scan_diff", details);
            audit_log.record_entry(AuditLog::Action::SCAN_DIFF, AuditLog::Status::SUCCESS,
                                  "Scanned " + std::to_string(stats.files.size()) + " changed files", details);
            audit_log.write_report();
            trace_logger.flush();
            std::cerr << "TRACE: scan-diff returning 0" << std::endl;
            return 0;
        } catch (const std::exception& e) {
            std::cerr << "Error: " << e.what() << '\n';
            audit_log.record_entry(AuditLog::Action::SCAN_DIFF, AuditLog::Status::FAILURE,
                                  std::string(e.what()));
            audit_log.write_report();
            std::cerr << "TRACE: scan-diff returning 1 (exception)" << std::endl;
            return 1;
        }
    }

    if (command == "validate-policy") {
        try {
            std::cerr << "TRACE: validate-policy start" << std::endl;
            ConfigLoader config_loader("config/forbidden-paths.json");
            config_loader.load();
            const auto& policies = config_loader.get_forbidden_paths();
            std::cerr << "TRACE: validate-policy config loaded" << std::endl;

            DiffScanner diff_scanner;
            auto diff_stats = diff_scanner.scan();
            std::cerr << "TRACE: validate-policy diff scanned" << std::endl;

            PathValidator validator(policies);
            auto validation = validator.validate_diff(diff_stats);
            validator.print_validation_result(validation);
            std::cerr << "TRACE: validate-policy validation done" << std::endl;
            
            json details;
            details["valid"] = validation.is_valid;
            details["violations"] = validation.violations.size();
                        auto meta_json = github_metadata.to_json();
                        for (const auto& meta : meta_json.items()) {
                            details[meta.key()] = meta.value();
                        }
            trace_logger.log_trace("validate_policy", details);
            
            AuditLog::Status status = validation.is_valid ? AuditLog::Status::SUCCESS : AuditLog::Status::FAILURE;
            audit_log.record_entry(AuditLog::Action::VALIDATE_POLICY, status,
                                  validation.is_valid ? "Policy validation passed" : "Policy violations detected", details);
            audit_log.write_report();
            trace_logger.flush();

            std::cerr << "TRACE: validate-policy returning " << (validation.is_valid ? 0 : 1) << std::endl;
            return validation.is_valid ? 0 : 1;
        } catch (const std::exception& e) {
            std::cerr << "Error: " << e.what() << '\n';
            audit_log.record_entry(AuditLog::Action::VALIDATE_POLICY, AuditLog::Status::FAILURE,
                                  std::string(e.what()));
            audit_log.write_report();
            std::cerr << "TRACE: validate-policy returning 1 (exception)" << std::endl;
            return 1;
        }
    }

    if (command == "validate-dependencies") {
        try {
            std::cerr << "TRACE: validate-dependencies start" << std::endl;

            DiffScanner diff_scanner;
            auto diff_stats = diff_scanner.scan();
            std::cerr << "TRACE: validate-dependencies diff scanned" << std::endl;

            DependencyValidator dep_validator("config/dependency-rules.json");
            dep_validator.load_rules();
            std::cerr << "TRACE: validate-dependencies dependency rules loaded" << std::endl;

            auto dep_violations = dep_validator.validate(diff_stats);
            dep_validator.print_violations(dep_violations);

            json details;
            details["dependency_violations"] = json::array();
            for (const auto& v : dep_violations) {
                json v_json;
                v_json["source_file"] = v.source_file;
                v_json["boundary"] = v.boundary_name;
                v_json["forbidden_dependency"] = v.forbidden_dependency;
                v_json["reason"] = v.reason;
                details["dependency_violations"].push_back(v_json);
            }

            auto meta_json = github_metadata.to_json();
            for (const auto& meta : meta_json.items()) {
                details[meta.key()] = meta.value();
            }
            trace_logger.log_trace("validate_dependencies", details);

            bool has_violations = !dep_violations.empty();
            AuditLog::Status log_status = has_violations
                                          ? AuditLog::Status::FAILURE
                                          : AuditLog::Status::SUCCESS;
            audit_log.record_entry(AuditLog::Action::VALIDATE_DEPENDENCIES, log_status,
                                  has_violations ? "Dependency violations found" : "Dependency validation passed",
                                  details);
            audit_log.write_report();
            trace_logger.flush();

            std::cerr << "TRACE: validate-dependencies returning " << (has_violations ? 1 : 0) << std::endl;
            return has_violations ? 1 : 0;
        } catch (const std::exception& e) {
            std::cerr << "Error: " << e.what() << '\n';
            audit_log.record_entry(AuditLog::Action::VALIDATE_DEPENDENCIES, AuditLog::Status::FAILURE,
                                  std::string(e.what()));
            audit_log.write_report();
            std::cerr << "TRACE: validate-dependencies returning 1 (exception)" << std::endl;
            return 1;
        }
    }

    std::cerr << "Unknown command: " << command << '\n';
    return 1;
}
