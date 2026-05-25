# Planner-CLI Architecture

This repository demonstrates a constrained orchestrator that produces planning
artifacts using an LLM while enforcing read-only boundaries and human-in-the-loop
workflow controls. Key design goals:

- Separation of concerns: scanner, planner, LLM client, git, and PR logic are split.
- Deterministic tooling: scanner sorts paths and uses temperature=0 for model.
- Bounded execution: LLM receives repository context only via prompt; no fs access.
- Auditability: outputs are markdown and PRs are created with the plan body.
- Observability: simple stage logs printed to stdout/stderr.

Modules:
- `cli` - minimal argument parsing
- `repository` - deterministic scanner for .cpp/.h/.hpp files
- `agents` - prompt builder and planner orchestration
- `llm` - minimal GitHub Models API client using libcurl
- `git` - branch creation and PR creation via GitHub REST API
- `output` - markdown sanitization
- `utils` - logging helpers

Security and Permissions:
- The tool runs locally and uses the user's git credentials for branch creation.
- GitHub interactions require `GITHUB_TOKEN` set in the environment.
- LLM access requires `GITHUB_MODELS_API_KEY` and optional `GITHUB_MODELS_API_URL`.
- The CLI automatically loads a local `.env` file from the repository root when present.

Usage:
1. Set environment variables `GITHUB_TOKEN` and `GITHUB_MODELS_API_KEY`.
2. Run `./planner-cli "Refactor checkout caching layer"` from repository root.

The orchestrator will scan the repo, build a constrained prompt, request a
markdown plan from the model, create a branch `ai/<short-task-name>`, and open
a pull request containing the plan as the PR body. Human reviewers then continue
the loop.
