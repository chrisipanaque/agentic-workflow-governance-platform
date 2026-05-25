// planner_agent.h
// Planner orchestration API: builds prompts and interacts with the LLM client.

#pragma once

#include <string>
#include "../repository/cpp_scanner.h"

namespace Planner {
    // Build a prompt from a human task and repository scan results.
    std::string build_prompt(const std::string& task, const Repository::ScanResult& scan);

    // Call the models client and return the raw markdown plan.
    std::string plan_from_model(const std::string& prompt);

    // Make a sanitized branch-safe short name for the task
    std::string sanitize_task_for_branch(const std::string& task);
}
