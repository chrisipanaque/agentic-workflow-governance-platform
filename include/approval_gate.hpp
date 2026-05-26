#pragma once

#include <string>
#include <vector>
#include "diff_scanner.hpp"
#include "risk_engine.hpp"
#include "github_metadata.hpp"

class ApprovalGate {
public:
    enum class Status {
        AUTO_APPROVED,
        REQUIRES_REVIEW,
        BLOCKED
    };

    struct ApprovalThresholds {
        double block = 75.0;
        double require_review = 25.0;
        double auto_approve = 10.0;
        int approvers_required = 1;
    };

    struct CodeOwner {
        std::string pattern;
        std::vector<std::string> owners;
    };

    struct ApprovalDecision {
        Status status = Status::AUTO_APPROVED;
        double score = 0.0;
        std::vector<std::string> required_approvers;
        std::vector<std::string> changed_components;
        std::string recommendation;
    };

    explicit ApprovalGate(const std::string& config_path);

    void load_config();
    void load_codeowners(const std::string& codeowners_path = ".github/CODEOWNERS");

    ApprovalDecision evaluate(const RiskEngine::RiskScore& risk,
                              const DiffScanner::DiffStats& diff_stats);

    std::string generate_markdown_report(const ApprovalDecision& decision,
                                          const GitHubMetadata::PRMetadata& pr_meta,
                                          const RiskEngine::RiskScore& risk) const;

    void print_decision(const ApprovalDecision& decision) const;
    Status score_to_status(double score) const;
    const ApprovalThresholds& get_thresholds() const { return thresholds_; }

private:
    std::string config_path_;
    ApprovalThresholds thresholds_;
    std::vector<CodeOwner> codeowners_;

    std::vector<std::string> find_owners_for_file(const std::string& file_path) const;
    bool path_matches_pattern(const std::string& path, const std::string& pattern) const;
    bool simple_glob_match(const std::string& text, const std::string& pattern) const;
};
