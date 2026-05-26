#include "approval_gate.hpp"
#include <nlohmann/json.hpp>
#include <fstream>
#include <iostream>
#include <filesystem>
#include <sstream>
#include <algorithm>

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
        if (t.contains("auto_approve")) thresholds_.auto_approve = t["auto_approve"].get<double>();
    }

    if (config.contains("approvers_required")) {
        thresholds_.approvers_required = config["approvers_required"].get<int>();
    }
}

void ApprovalGate::load_codeowners(const std::string& codeowners_path) {
    codeowners_.clear();

    std::string path = codeowners_path;
    std::ifstream file(path);
    if (!file.is_open()) {
        // Try fallback location
        path = "CODEOWNERS";
        file.open(path);
    }
    if (!file.is_open()) {
        std::cerr << "Warning: No CODEOWNERS file found, owners will be empty\n";
        return;
    }

    std::string line;
    while (std::getline(file, line)) {
        // Strip trailing whitespace
        while (!line.empty() && (line.back() == ' ' || line.back() == '\t' || line.back() == '\r')) {
            line.pop_back();
        }
        if (line.empty() || line[0] == '#') continue;

        std::istringstream iss(line);
        std::string pattern;
        iss >> pattern;
        if (pattern.empty()) continue;

        std::vector<std::string> owners;
        std::string owner;
        while (iss >> owner) {
            if (owner[0] == '@') {
                owners.push_back(owner);
            }
        }

        if (!owners.empty()) {
            codeowners_.push_back({pattern, owners});
        }
    }
}

bool ApprovalGate::simple_glob_match(const std::string& text, const std::string& pattern) const {
    size_t text_idx = 0;
    size_t pattern_idx = 0;
    size_t star_idx = std::string::npos;
    size_t match_idx = 0;

    while (text_idx < text.length()) {
        if (pattern_idx < pattern.length()) {
            if (pattern[pattern_idx] == '*') {
                star_idx = pattern_idx;
                match_idx = text_idx;
                pattern_idx++;
                continue;
            } else if (pattern[pattern_idx] == '?' || pattern[pattern_idx] == text[text_idx]) {
                pattern_idx++;
                text_idx++;
                continue;
            }
        }

        if (star_idx != std::string::npos) {
            pattern_idx = star_idx + 1;
            match_idx++;
            text_idx = match_idx;
        } else {
            return false;
        }
    }

    while (pattern_idx < pattern.length() && pattern[pattern_idx] == '*') {
        pattern_idx++;
    }

    return pattern_idx == pattern.length();
}

bool ApprovalGate::path_matches_pattern(const std::string& path, const std::string& pattern) const {
    if (pattern.find('*') == std::string::npos && pattern.find('?') == std::string::npos) {
        return path == pattern || path.find(pattern) == 0;
    }
    return simple_glob_match(path, pattern);
}

std::vector<std::string> ApprovalGate::find_owners_for_file(const std::string& file_path) const {
    // CODEOWNERS convention: last matching pattern wins (most specific)
    std::vector<std::string> owners;
    for (const auto& entry : codeowners_) {
        if (path_matches_pattern(file_path, entry.pattern)) {
            owners = entry.owners;
        }
    }
    return owners;
}

ApprovalGate::ApprovalDecision ApprovalGate::evaluate(
        const RiskEngine::RiskScore& risk,
        const DiffScanner::DiffStats& diff_stats) {

    ApprovalDecision decision;
    decision.score = risk.score;
    decision.status = score_to_status(risk.score);

    // Collect all unique required approvers from changed files
    for (const auto& file : diff_stats.files) {
        decision.changed_components.push_back(file.file_path);
        auto owners = find_owners_for_file(file.file_path);
        for (const auto& owner : owners) {
            if (std::find(decision.required_approvers.begin(),
                          decision.required_approvers.end(), owner) == decision.required_approvers.end()) {
                decision.required_approvers.push_back(owner);
            }
        }
    }

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

    if (!decision.changed_components.empty()) {
        md << "## Changed Files\n\n";
        md << "| File | Required Approvers |\n";
        md << "|---|---|\n";
        for (const auto& file : decision.changed_components) {
            auto owners = find_owners_for_file(file);
            std::string owner_str;
            for (size_t i = 0; i < owners.size(); ++i) {
                if (i > 0) owner_str += ", ";
                owner_str += owners[i];
            }
            if (owner_str.empty()) owner_str = "(no owners)";
            md << "| " << file << " | " << owner_str << " |\n";
        }
        md << "\n";
    }

    if (!decision.required_approvers.empty()) {
        md << "## Required Approvers\n\n";
        md << "- **" << thresholds_.approvers_required << "** approval(s) required\n";
        md << "- Minimum **" << static_cast<int>(decision.score) << "/100** risk threshold triggered\n\n";
        md << "Required reviewers:\n";
        for (const auto& approver : decision.required_approvers) {
            md << "- " << approver << "\n";
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

    if (!decision.required_approvers.empty()) {
        std::cout << "Required approvers:\n";
        for (const auto& approver : decision.required_approvers) {
            std::cout << "  - " << approver << "\n";
        }
    }
}
