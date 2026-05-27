#include "approval_gate.hpp"
#include <nlohmann/json.hpp>
#include <fstream>
#include <iostream>
#include <sstream>

using json = nlohmann::json;

ApprovalGate::ApprovalGate(const std::string& config_path)
    : config_path_(config_path) {}

void ApprovalGate::load_config() {
    std::ifstream file(config_path_);
    if (!file.is_open()) {
        std::cerr << "Warning: Failed to open approval config: " << config_path_
                  << ", using defaults\n";
        return;
    }

    json config = json::parse(file);

    if (config.contains("thresholds")) {
        const auto& t = config["thresholds"];
        if (t.contains("block")) thresholds_.block = t["block"].get<double>();
        if (t.contains("require_review")) thresholds_.require_review = t["require_review"].get<double>();
    }

    if (config.contains("approvers_required")) {
        thresholds_.approvers_required = config["approvers_required"].get<int>();
    }
}

ApprovalGate::ApprovalDecision ApprovalGate::evaluate(
        const RiskEngine::RiskScore& risk,
        const DiffScanner::DiffStats&) {

    ApprovalDecision decision;
    decision.score = risk.score;
    decision.status = score_to_status(risk.score);

    switch (decision.status) {
        case Status::BLOCKED:
            decision.recommendation = "BLOCK: Do not approve. Escalate to security team.";
            break;
        case Status::REQUIRES_REVIEW:
            decision.recommendation = "REVIEW: Manual review required before approval.";
            break;
        case Status::AUTO_APPROVED:
            decision.recommendation = "PROCEED: Standard review sufficient.";
            break;
    }

    return decision;
}

ApprovalGate::Status ApprovalGate::score_to_status(double score) const {
    if (score >= thresholds_.block) return Status::BLOCKED;
    if (score >= thresholds_.require_review) return Status::REQUIRES_REVIEW;
    return Status::AUTO_APPROVED;
}

std::string ApprovalGate::generate_markdown_report(
        const ApprovalDecision& decision,
        const GitHubMetadata::PRMetadata& pr_meta,
        const RiskEngine::RiskScore& risk) const {

    std::ostringstream md;

    md << "# Approval Report\n\n";

    md << "**Repository**: " << pr_meta.repository << "\n";
    if (!pr_meta.pr_number.empty()) {
        md << "**PR**: #" << pr_meta.pr_number << "\n";
    }
    md << "**Branch**: " << pr_meta.branch << "\n";
    md << "**Commit**: " << pr_meta.commit_sha.substr(0, 8) << "\n";
    md << "**Author**: @" << pr_meta.actor << "\n\n";

    md << "## Risk Assessment\n\n";
    md << "| Metric | Value |\n";
    md << "|---|---|\n";
    md << "| Risk Score | " << static_cast<int>(decision.score) << "/100 |\n";

    const char* severity_str = "UNKNOWN";
    switch (risk.severity) {
        case RiskEngine::Severity::LOW: severity_str = "LOW"; break;
        case RiskEngine::Severity::MEDIUM: severity_str = "MEDIUM"; break;
        case RiskEngine::Severity::HIGH: severity_str = "HIGH"; break;
        case RiskEngine::Severity::CRITICAL: severity_str = "CRITICAL"; break;
    }
    md << "| Severity | " << severity_str << " |\n";
    md << "| Recommendation | " << risk.recommendation << " |\n\n";

    if (!risk.risk_factors.empty()) {
        md << "## Risk Factors\n\n";
        for (const auto& factor : risk.risk_factors) {
            md << "- " << factor << "\n";
        }
        md << "\n";
    }

    md << "## Decision\n\n";
    const char* status_str = "UNKNOWN";
    switch (decision.status) {
        case Status::AUTO_APPROVED: status_str = "AUTO-APPROVED"; break;
        case Status::REQUIRES_REVIEW: status_str = "REQUIRES REVIEW"; break;
        case Status::BLOCKED: status_str = "BLOCKED"; break;
    }
    md << "**" << status_str << "**\n\n";
    md << decision.recommendation << "\n";

    return md.str();
}

void ApprovalGate::print_decision(const ApprovalDecision& decision) const {
    const char* status_str = "UNKNOWN";
    switch (decision.status) {
        case Status::AUTO_APPROVED: status_str = "AUTO-APPROVED"; break;
        case Status::REQUIRES_REVIEW: status_str = "REQUIRES_REVIEW"; break;
        case Status::BLOCKED: status_str = "BLOCKED"; break;
    }

    std::cout << "Approval Status: " << status_str << "\n";
    std::cout << "Risk Score: " << static_cast<int>(decision.score) << "/100\n";
    std::cout << "Recommendation: " << decision.recommendation << "\n";
}
