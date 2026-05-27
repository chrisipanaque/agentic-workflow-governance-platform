# agentic-workflow-governance-tools

Deterministic C++20 CLI that intercepts AI agent code changes at the repository boundary, validates them against configurable policies, and returns structured decisions via exit codes — all with full traceability.

All 5 commands run as standalone CLI tools in any git repository — same binary, same exit codes whether you run them locally in your terminal or as a CI gate.

## Requirements

- **Git** — the tool runs `git diff` and installs as a git submodule
- **CMake 3.20+** — C++20 build system
  - macOS: `brew install cmake`
  - Linux (Debian/Ubuntu): `sudo apt-get install cmake build-essential`
- **C++20 compiler** — GCC 10+, Clang 10+, Apple Clang 14+, or MSVC 2022+
- **No package manager needed** — the sole dependency (`nlohmann_json` v3.11.2) fetches automatically via CMake FetchContent
- **Platforms**: Linux (x86_64, aarch64), macOS (arm64, x86_64)

---

## Quick Start

Run this inside your repo to set up the governance tool as a git submodule, build it, and create a CI workflow:

```sh
curl -fsSL https://raw.githubusercontent.com/chrisipanaque/agentic-workflow-governance-tools/main/setup.sh | bash
```

After setup, test it locally:

```sh
./_gov/build/agentic-workflow-governance-tools validate-policy
```

Build instructions, command details, and development notes are in `AGENTS.md`.

---

## Agent System Architecture

The agent workflow governance tools implement a **synchronous multi-stage pipeline** that transforms a raw git diff into a governance decision. Every stage produces a deterministic output consumed by the next:

```
                       CONFIGURATION LAYER
                    ┌────────────────────────┐
                    │  forbidden-paths.json   │
                    │  dependency-rules.json  │
                    └────────┬───────────────┘
                             │
    PIPELINE                 │
                             │
  [Git Diff]                 │
      │                      │
      ▼                      │
  ┌─────────────┐            │
  │ DiffScanner │────────────┘
  └──────┬──────┘
         │
         ▼
  ┌──────────────────┐
  │  Policy Layer     │
  │                   │
  │  PathValidator    │◄──── config/forbidden-paths.json
  │  DependencyValid. │◄──── config/dependency-rules.json
  └──────┬───────────┘
         │
         ▼
  ┌──────────────────────┐
  │  Observability       │
  │                      │
  │  audit_*.json        │
  │  trace_*.jsonl       │
  └──────────────────────┘
```

### Components

| Component | Role | Input | Output |
|---|---|---|---|
| `DiffScanner` | Captures what the agent changed | `git diff --unified=0` | `DiffStats { files[], +/- counts }` |
| `ConfigLoader` | Ingests policy configs | `forbidden-paths.json` | `ForbiddenPath[]` |
| `PathValidator` | Hard-blocks forbidden file paths | `ForbiddenPath[]` + `DiffStats` | `ValidationResult` (pass/fail) |
| `DependencyValidator` | Scans `#include` directives for cross-module boundary violations | `dependency-rules.json` + `DiffStats` | `Violation[]` |
| `GitHubMetadata` | Enriches all outputs with CI context | `GITHUB_*` env vars | `PRMetadata` + JSON |
| `AuditLog` | Structured JSON history of every command execution | Action + Status + details | `output/reports/audit_*.json` |
| `TraceLogger` | JSONL event stream for real-time observability | event_type + data | `output/traces/trace_*.jsonl` |

### Exit Code Protocol

Every command returns 0 or 1, forming the sole communication channel from the governance tool back to the agent or CI runner:

| Command | Exit 0 | Exit 1 |
|---|---|---|---|
| `healthcheck` | always | exception |
| `show-policies` | always | exception |
| `scan-diff` | always | exception |
| `validate-policy` | no forbidden paths touched | violations found |
| `validate-dependencies` | no dependency violations | dependency violations found |
| *(unknown)* | — | 1 |

Exit 0 signals "safe to proceed automatically." Exit 1 signals "requires human intervention."

---

## Core Agentic Workflow

### Stage 1: Diff Scanning

When an AI agent (Claude Code, Copilot, etc.) makes changes to the repository, the governance tool captures exactly what changed by running `git --no-pager diff --unified=0` via `popen()`. This captures both staged and unstaged changes.

**Output**: `DiffStats` — a structured list of every changed file with per-file addition/deletion counts and aggregate totals.

### Stage 2: Policy Validation

Each changed file path is checked against the forbidden-path rules using glob matching (`*`/`?`). If any path matches a forbidden pattern, the command exits 1 and the agent receives a hard block.

**Policy config**: `config/forbidden-paths.json` — 3 entries (system files, SSH keys, security logs).

### Stage 3: Dependency Validation

For each changed `.cpp`/`.hpp` file that matches a boundary source pattern, the validator reads the file from disk and parses every `#include "..."` directive. If any included path starts with a forbidden dependency prefix, a violation is recorded and the command exits 1.

**Policy config**: `config/dependency-rules.json` — 3 boundaries (core↔plugin isolation, frontend↔backend isolation, security isolation).

### Stage 4: Observability

Every command execution writes to two output channels:

| Channel | Format | Location | Purpose |
|---|---|---|---|
| Audit log | Pretty-printed JSON array | `output/reports/audit_*.json` | Post-hoc analysis, historical records |
| Trace log | JSONL (one JSON object per line, append) | `output/traces/trace_*.jsonl` | Real-time streaming, CI observability |

Both are flushed synchronously before the process exits. Directory creation uses `std::filesystem::create_directories()` (no shell forking).

---

## Policy Configuration Architecture

Two JSON config files govern all policy behavior. No recompilation is needed to adjust any policy — the agent or operator edits JSON directly.

| Config File | Domain | Rules | Evaluated By | Changes Behavior |
|---|---|---|---|---|
| `config/forbidden-paths.json` | Security (hard block) | 3 path patterns | `PathValidator` | What files agents may not touch |
| `config/dependency-rules.json` | Architectural isolation | 3 boundaries | `DependencyValidator` | Which cross-module includes are forbidden |

---

## Agent Integration Patterns

### Exit Code as Decision Signal

The governance tool follows Unix convention: 0 = success, non-zero = action required. CI pipelines and agent scripts use simple `if`/`else` on exit codes:

```sh
./build/agentic-workflow-governance-tools validate-policy
if [ $? -eq 0 ]; then
  echo "Policy check passed — safe to proceed"
else
  echo "Policy violation — requires human intervention"
fi
```

### Trace/Audit Feedback Loop

After execution, agents can read `audit_*.json` to understand *why* a policy fired — which file, which rule. This enables self-diagnosis and adjustment of future behavior.

### Config-Driven Governance

All policies are JSON files. Agents can edit any config file to adjust enforcement behavior without recompiling the binary. This decouples governance logic (C++ binary) from policy definition (JSON files).

### Single-Binary Deployment

The compiled binary (`agentic-workflow-governance-tools`) and its two config files are the only deployable units. Zero runtime dependencies. This means any agent environment — local dev machine, CI runner, container — can enforce the same policies by receiving the same binary and configs.

---

## Quick Reference

```
CLI:  ./build/agentic-workflow-governance-tools <command>
C++:  C++20, CMake 3.20+
Dep:  nlohmann/json v3.11.2 (FetchContent, no package manager)
LOC:  1,048 lines across 8 source + 7 header files
CI:   GitHub Actions — 10-step smoke test workflow
```
