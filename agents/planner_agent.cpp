// planner_agent.cpp
// Responsible for building a constrained prompt and invoking the LLM client.
// The LLM is treated as a black-box service: it receives a prompt and returns
// markdown. The LLM MUST NOT have filesystem access; all repository context
// is provided as prompt text only.

#include "planner_agent.h"
#include "../llm/github_models_client.h"
#include "../output/markdown_generator.h"
#include <sstream>
#include <algorithm>
#include <cctype>

namespace Planner {

    std::string build_prompt(const std::string& task, const Repository::ScanResult& scan) {
        // Build a deterministic prompt that provides repository context and
        // constrained instructions for the planner agent.
        std::ostringstream ss;
        ss << "You are a bounded planner agent.\n";
        ss << "Constraints: read-only analysis. DO NOT propose code changes inline.\n";
        ss << "Only produce a Markdown planning artifact with sections: Goal, Affected Components, Proposed Changes, Risks, Testing Strategy.\n\n";
        ss << "Task: " << task << "\n\n";

        ss << "Repository context (file snippets):\n";
        for (const auto &f : scan.files) {
            ss << "---\nFile: " << f.path << "\n";
            // Provide only a small deterministic snippet
            ss << "```cpp\n";
            ss << f.snippet;
            ss << "```\n";
        }

        ss << "\nRemember: output MARKDOWN only. No code edits, no commands.\n";
        return ss.str();
    }

    std::string plan_from_model(const std::string& prompt) {
        // Use the GitHub Models client to send the prompt and retrieve a completion.
        LLM::Client client;
        LLM::Request req;
        req.prompt = prompt;
        req.max_tokens = 1500; // bounded
        req.temperature = 0.0; // deterministic planning

        LLM::Response resp = client.complete(req);
        // Sanitize and enforce markdown-only artifact
        return Output::sanitize_markdown(resp.text);
    }

    std::string sanitize_task_for_branch(const std::string& task) {
        // Make a short, lower-case, dash-separated token from task
        std::string out;
        for (char c : task) {
            if (std::isalnum((unsigned char)c)) out += std::tolower(c);
            else if (std::isspace((unsigned char)c) || c=='_' || c=='-') out += '-';
            // drop other punctuation
        }
        // compress runs of '-'
        std::string comp;
        bool prev_dash = false;
        for (char c : out) {
            if (c == '-') {
                if (!prev_dash) { comp += c; prev_dash = true; }
            } else { comp += c; prev_dash = false; }
        }
        if (comp.size() > 40) comp = comp.substr(0,40);
        // trim leading/trailing '-'
        while (!comp.empty() && comp.front()=='-') comp.erase(comp.begin());
        while (!comp.empty() && comp.back()=='-') comp.pop_back();
        if (comp.empty()) comp = "task";
        return comp;
    }

}
