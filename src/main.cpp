#include <iostream>
#include <fstream>
#include <chrono>
#include <string_view>
#include "config_loader.hpp"
#include "diff_scanner.hpp"
#include "path_validator.hpp"
#include "risk_engine.hpp"
#include "trace_logger.hpp"
#include "audit_log.hpp"
#include "github_metadata.hpp"
#include "approval_gate.hpp"
#include "dependency_validator.hpp"
#include "ownership_validator.hpp"

int main(int argc, char* argv[]) {
    std::cerr << "TRACE: main entered" << std::endl;
    if (argc < 2) {
        std::cerr << "Usage: ai-governance-platform <command>\n";
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
    auto pr_meta = github_metadata.get_pr_metadata();
    std::cerr << "TRACE: PR metadata obtained" << std::endl;

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

    if (command == "risk-score") {
        try {
            std::cerr << "TRACE: risk-score start" << std::endl;
            DiffScanner diff_scanner;
            auto diff_stats = diff_scanner.scan();
            std::cerr << "TRACE: risk-score diff scanned" << std::endl;

            RiskEngine engine("config/risk-rules.json");
            engine.load_rules();
            auto risk = engine.calculate_score(diff_stats);
            engine.print_score(risk);
            std::cerr << "TRACE: risk-score calculated" << std::endl;
            
            json details;
            details["score"] = risk.score;
            details["severity"] = risk.severity == RiskEngine::Severity::CRITICAL ? "CRITICAL" :
                                   risk.severity == RiskEngine::Severity::HIGH ? "HIGH" :
                                   risk.severity == RiskEngine::Severity::MEDIUM ? "MEDIUM" : "LOW";
            details["recommendation"] = risk.recommendation;
            details["risk_factors"] = risk.risk_factors;
                        auto meta_json = github_metadata.to_json();
                        for (const auto& meta : meta_json.items()) {
                            details[meta.key()] = meta.value();
                        }
            trace_logger.log_trace("risk_score", details);
            
            AuditLog::Status status = (risk.score >= 75.0) ? AuditLog::Status::FAILURE : AuditLog::Status::SUCCESS;
            audit_log.record_entry(AuditLog::Action::RISK_SCORE, status,
                                  "Risk score: " + std::to_string(static_cast<int>(risk.score)), details);
            audit_log.write_report();
            trace_logger.flush();

            std::cerr << "TRACE: risk-score returning " << ((risk.score >= 75.0) ? 1 : 0) << std::endl;
            return (risk.score >= 75.0) ? 1 : 0;
        } catch (const std::exception& e) {
            std::cerr << "Error: " << e.what() << '\n';
            audit_log.record_entry(AuditLog::Action::RISK_SCORE, AuditLog::Status::FAILURE,
                                  std::string(e.what()));
            audit_log.write_report();
            std::cerr << "TRACE: risk-score returning 1 (exception)" << std::endl;
            return 1;
        }
    }

    if (command == "check-approval") {
        try {
            std::cerr << "TRACE: check-approval start" << std::endl;
            DiffScanner diff_scanner;
            auto diff_stats = diff_scanner.scan();
            std::cerr << "TRACE: check-approval diff scanned" << std::endl;

            RiskEngine engine("config/risk-rules.json");
            engine.load_rules();
            auto risk = engine.calculate_score(diff_stats);
            std::cerr << "TRACE: check-approval risk calculated, score=" << risk.score << std::endl;

            ApprovalGate gate("config/approval-config.json");
            gate.load_config();
            gate.load_codeowners(".github/CODEOWNERS");
            std::cerr << "TRACE: check-approval config loaded" << std::endl;

            auto decision = gate.evaluate(risk, diff_stats);
            gate.print_decision(decision);
            std::cerr << "TRACE: check-approval evaluated" << std::endl;

            auto md_report = gate.generate_markdown_report(decision, pr_meta, risk);
            std::cerr << "TRACE: check-approval markdown generated" << std::endl;

            std::filesystem::create_directories("output/reports");
            auto now = std::chrono::system_clock::now();
            auto time_t_now = std::chrono::system_clock::to_time_t(now);
            char filename[64];
            std::strftime(filename, sizeof(filename), "approval_%Y%m%d_%H%M%S.md",
                          std::localtime(&time_t_now));
            std::string filepath = std::string("output/reports/") + filename;
            std::ofstream md_file(filepath);
            if (md_file.is_open()) {
                md_file << md_report;
                md_file.close();
                std::cerr << "TRACE: check-approval report written to " << filepath << std::endl;
            }

            json details;
            details["score"] = risk.score;
            details["approval_status"] = decision.status == ApprovalGate::Status::AUTO_APPROVED ? "AUTO_APPROVED" :
                                          decision.status == ApprovalGate::Status::REQUIRES_REVIEW ? "REQUIRES_REVIEW" : "BLOCKED";
            details["required_approvers"] = decision.required_approvers;
            details["changed_files"] = diff_stats.files.size();
            auto meta_json = github_metadata.to_json();
            for (const auto& meta : meta_json.items()) {
                details[meta.key()] = meta.value();
            }
            trace_logger.log_trace("check_approval", details);

            AuditLog::Status log_status = (decision.status == ApprovalGate::Status::AUTO_APPROVED)
                                          ? AuditLog::Status::SUCCESS : AuditLog::Status::FAILURE;
            audit_log.record_entry(AuditLog::Action::CHECK_APPROVAL, log_status,
                                  decision.recommendation, details);
            audit_log.write_report();
            trace_logger.flush();

            std::cerr << "TRACE: check-approval returning " << ((decision.status == ApprovalGate::Status::AUTO_APPROVED) ? 0 : 1) << std::endl;
            return (decision.status == ApprovalGate::Status::AUTO_APPROVED) ? 0 : 1;
        } catch (const std::exception& e) {
            std::cerr << "Error: " << e.what() << '\n';
            audit_log.record_entry(AuditLog::Action::CHECK_APPROVAL, AuditLog::Status::FAILURE,
                                  std::string(e.what()));
            audit_log.write_report();
            std::cerr << "TRACE: check-approval returning 1 (exception)" << std::endl;
            return 1;
        }
    }

    if (command == "validate-architecture") {
        try {
            std::cerr << "TRACE: validate-architecture start" << std::endl;

            DiffScanner diff_scanner;
            auto diff_stats = diff_scanner.scan();
            std::cerr << "TRACE: validate-architecture diff scanned" << std::endl;

            DependencyValidator dep_validator("config/dependency-rules.json");
            dep_validator.load_rules();
            std::cerr << "TRACE: validate-architecture dependency rules loaded" << std::endl;

            OwnershipValidator own_validator("config/ownership-rules.json");
            own_validator.load_rules();
            std::cerr << "TRACE: validate-architecture ownership rules loaded" << std::endl;

            auto dep_violations = dep_validator.validate(diff_stats);
            dep_validator.print_violations(dep_violations);

            auto ownership_info = own_validator.get_file_ownership(diff_stats);
            own_validator.print_ownership(ownership_info);

            auto touched_boundaries = own_validator.detect_cross_boundary(diff_stats);
            own_validator.print_cross_boundary(touched_boundaries);

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
            details["touched_boundaries"] = json(touched_boundaries);

            auto meta_json = github_metadata.to_json();
            for (const auto& meta : meta_json.items()) {
                details[meta.key()] = meta.value();
            }
            trace_logger.log_trace("validate_architecture", details);

            bool has_violations = !dep_violations.empty();
            AuditLog::Status log_status = has_violations
                                          ? AuditLog::Status::FAILURE
                                          : AuditLog::Status::SUCCESS;
            audit_log.record_entry(AuditLog::Action::VALIDATE_ARCHITECTURE, log_status,
                                  has_violations ? "Dependency violations found" : "Architecture validation passed",
                                  details);
            audit_log.write_report();
            trace_logger.flush();

            std::cerr << "TRACE: validate-architecture returning " << (has_violations ? 1 : 0) << std::endl;
            return has_violations ? 1 : 0;
        } catch (const std::exception& e) {
            std::cerr << "Error: " << e.what() << '\n';
            audit_log.record_entry(AuditLog::Action::VALIDATE_ARCHITECTURE, AuditLog::Status::FAILURE,
                                  std::string(e.what()));
            audit_log.write_report();
            std::cerr << "TRACE: validate-architecture returning 1 (exception)" << std::endl;
            return 1;
        }
    }

    std::cerr << "Unknown command: " << command << '\n';
    return 1;
}
