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
        int approvers_required = 1;
    };

    struct ApprovalDecision {
        Status status = Status::AUTO_APPROVED;
        double score = 0.0;
        std::string recommendation;
    };

    explicit ApprovalGate(const std::string& config_path);

    void load_config();

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
};
