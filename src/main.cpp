// main.cpp
// Orchestrator entrypoint for the planner-cli tool.
// This program wires together the CLI parser, repository scanner,
// planner agent, git managers, and GitHub integrations to produce
// a human-in-the-loop planning PR workflow.

#include <iostream>
#include <cstdlib>
#include <string>

#include "../cli/cli_parser.h"
#include "../repository/cpp_scanner.h"
#include "../agents/planner_agent.h"
#include "../git/branch_manager.h"
#include "../git/github_pr.h"
#include "../utils/logger.h"
#include "../utils/env_loader.h"

int main(int argc, char** argv) {
    Logger::info("[CLI] parsing arguments...");
    std::string task = CLI::parse(argc, argv);
    if (task.empty()) {
        Logger::error("No task provided. Usage: planner-cli \"task description\"");
        return 1;
    }

    Env::load_dotenv(".env");

    Logger::info("[SCAN] scanning repository...");
    Repository::ScanResult scan = Repository::scan_cpp_files(".");

    Logger::info("[PROMPT] building planner prompt...");
    std::string prompt = Planner::build_prompt(task, scan);

    Logger::info("[PLAN] generating implementation plan via LLM...");
    std::string plan_markdown = Planner::plan_from_model(prompt);

    Logger::info("[GIT] creating branch...");
    std::string short_name = Planner::sanitize_task_for_branch(task);
    std::string branch = "ai/" + short_name;
    bool branch_ok = Git::create_branch(branch);
    if (!branch_ok) {
        Logger::error("Failed to create git branch: " + branch);
        return 1;
    }

    Logger::info("[PR] creating GitHub pull request...");
    bool pr_ok = GitHubPR::create_pull_request(task, plan_markdown, branch);
    if (!pr_ok) {
        Logger::error("Failed to create pull request.");
        return 1;
    }

    Logger::info("Workflow complete. Review the PR and continue human-in-the-loop work.");
    return 0;
}
