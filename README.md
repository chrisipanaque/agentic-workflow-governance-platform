# Agentic Governance Platform

Deterministic C++20 CLI that intercepts AI agent code changes at the repository boundary, validates them against configurable policies, and returns structured decisions via exit codes — all with full traceability.

---

## Agent System Architecture

The control plane implements a **synchronous multi-stage pipeline** that transforms a raw git diff into a governance decision. Every stage produces a deterministic output consumed by the next:

```
                        CONFIGURATION LAYER
                     ┌──────────────────────┐
                     │  forbidden-paths.json │
                     │  risk-rules.json      │
                     │  approval-config.json │
                     │  dependency-rules.json│
                     │  ownership-rules.json │
                     │  CODEOWNERS           │
                     └──────┬───────┬───────┘
                            │       │
    PIPELINE                │       │
                            │       │
  [Git Diff]                │       │
      │                     │       │
      ▼                     │       │
  ┌─────────────┐           │       │
  │ DiffScanner │───────────┼───────┘
  └──────┬──────┘           │
         │                  │
         ▼                  │
  ┌──────────────────┐      │
  │  Policy Layer     │      │
  │                   │      │
  │  PathValidator    │◄─────┘
  │  DependencyValid. │
  │  OwnershipValid.  │
  └──────┬───────────┘
         │
         ▼
  ┌─────────────┐
  │ RiskEngine  │─── RiskScore (0–100)
  └──────┬──────┘
         │
         ▼
  ┌──────────────┐
  │ ApprovalGate │─── AUTO_APPROVED / REQUIRES_REVIEW / BLOCKED
  └──────┬───────┘
         │
         ▼
  ┌─────────────────────┐
  │  Observability       │
  │                      │
  │  audit_*.json        │
  │  trace_*.jsonl       │
  │  approval_*.md       │
  └──────────────────────┘
```

### Components

| Component | Role | Input | Output |
|---|---|---|---|
| `DiffScanner` | Captures what the agent changed | `git diff --unified=0` | `DiffStats { files[], +/- counts }` |
| `ConfigLoader` | Ingests policy configs | `forbidden-paths.json` | `ForbiddenPath[]` |
| `PathValidator` | Hard-blocks forbidden file paths | `ForbiddenPath[]` + `DiffStats` | `ValidationResult` (pass/fail) |
| `RiskEngine` | Quantifies change risk on a 0–100 scale | `risk-rules.json` + `DiffStats` | `RiskScore { score, severity, factors }` |
| `ApprovalGate` | Determines approval tier + required reviewers | `approval-config.json` + `CODEOWNERS` + `RiskScore` | `ApprovalDecision` + markdown report |
| `DependencyValidator` | Enforces module boundary isolation by scanning `#include` directives | `dependency-rules.json` + `DiffStats` | `Violation[]` |
| `OwnershipValidator` | Maps changed files to team ownership boundaries | `ownership-rules.json` + `DiffStats` | `OwnershipInfo[]`, cross-boundary report |
| `GitHubMetadata` | Enriches all outputs with CI context | `GITHUB_*` env vars | `PRMetadata` + JSON |
| `AuditLog` | Structured JSON history of every command execution | Action + Status + details | `output/reports/audit_*.json` |
| `TraceLogger` | JSONL event stream for real-time observability | event_type + data | `output/traces/trace_*.jsonl` |

### Exit Code Protocol

Every command returns 0 or 1, forming the sole communication channel from the control plane back to the agent or CI runner:

| Command | Exit 0 | Exit 1 |
|---|---|---|
| `healthcheck` | always | exception |
| `show-policies` | always | exception |
| `scan-diff` | always | exception |
| `validate-policy` | no forbidden paths touched | violations found |
| `risk-score` | score < 75 (not CRITICAL) | score ≥ 75 (CRITICAL) |
| `check-approval` | AUTO_APPROVED | REQUIRES_REVIEW or BLOCKED |
| `validate-architecture` | no dependency violations | dependency violations found |
| *(unknown)* | — | 1 |

Exit 0 signals "safe to proceed automatically." Exit 1 signals "requires human intervention."

---

## Core Agentic Workflow

### Stage 1: Diff Scanning

When an AI agent (Claude Code, Copilot, etc.) makes changes to the repository, the control plane captures exactly what changed by running `git --no-pager diff --unified=0` via `popen()`. This captures both staged and unstaged changes.

**Output**: `DiffStats` — a structured list of every changed file with per-file addition/deletion counts and aggregate totals.

### Stage 2: Policy Validation

Each changed file path is checked against the forbidden-path rules using glob matching (`*`/`?`). If any path matches a forbidden pattern, the command exits 1 and the agent receives a hard block.

**Policy config**: `config/forbidden-paths.json` — 3 entries (system files, SSH keys, security logs).

### Stage 3: Risk Scoring

Every changed file is evaluated against 9 weighted risk rules across three categories:

| Category | Examples | Weight Range |
|---|---|---|
| Forbidden Path | `/etc/shadow`, `/root/.ssh` | 30–40 |
| Auth/Sensitive | `*.key`, `*secret*`, `*.pem` | 20–25 |
| Infrastructure | `*docker*`, `*kubernetes*`, `*.tf` | 12–15 |

Additionally, diff size contributes risk: >50 changes → +2, >100 → +5, >500 → +10, >1000 → +15.

**Score → Severity mapping**: CRITICAL ≥ 75, HIGH ≥ 50, MEDIUM ≥ 25, LOW < 25.

### Stage 4: Approval Gating

The risk score is mapped to a three-tier approval decision against configurable thresholds:

```
  score ≥ 75  ──→  BLOCKED         ──→ "Do not approve. Escalate."
  score ≥ 25  ──→  REQUIRES_REVIEW ──→ "Manual review required."
  score < 25  ──→  AUTO_APPROVED   ──→ "Standard review sufficient."
```

For non-auto-approved PRs, the system consults CODEOWNERS to identify required approvers based on which files changed. A human-readable markdown report is generated with per-file ownership, risk breakdown, and required reviewer list.

**Policy config**: `config/approval-config.json` (thresholds: block=75, require_review=25, auto_approve=10).

### Stage 5: Architecture Validation

Two parallel validators assess architectural impact:

**Dependency boundaries** — For each changed `.cpp`/`.hpp` file that matches a boundary source pattern, the validator reads the file from disk and parses every `#include "..."` directive. If any included path starts with a forbidden dependency prefix, a violation is recorded.

**Policy config**: `config/dependency-rules.json` — 3 boundaries (core↔plugin isolation, frontend↔backend isolation, security isolation).

**Ownership boundaries** — Each changed file is mapped to a team boundary by directory prefix matching. If changes span multiple ownership zones, a cross-boundary warning is issued (informational only — does not fail the command).

**Policy config**: `config/ownership-rules.json` — 4 boundaries (frontend, backend, core, config).

### Stage 6: Observability

Every command execution writes to three output channels:

| Channel | Format | Location | Purpose |
|---|---|---|---|
| Audit log | Pretty-printed JSON array | `output/reports/audit_*.json` | Post-hoc analysis, historical records |
| Trace log | JSONL (one JSON object per line, append) | `output/traces/trace_*.jsonl` | Real-time streaming, CI observability |
| Approval report | Markdown | `output/reports/approval_*.md` | Human-readable gating summary |

All three are flushed synchronously before the process exits. Directory creation uses `std::filesystem::create_directories()` (no shell forking).

---

## Policy Configuration Architecture

Five JSON config files and one CODEOWNERS file govern all system behavior. No recompilation is needed to adjust any policy — the agent or operator edits JSON directly.

| Config File | Domain | Rules | Evaluated By | Changes Behavior |
|---|---|---|---|---|
| `config/forbidden-paths.json` | Security (hard block) | 3 path patterns | `PathValidator` | What files agents may not touch |
| `config/risk-rules.json` | Risk quantification | 9 weighted patterns | `RiskEngine` | How risky a change is considered |
| `config/approval-config.json` | Decision thresholds | 3 thresholds | `ApprovalGate` | When auto-approval / review / block triggers |
| `.github/CODEOWNERS` | Review routing | 3 ownership patterns | `ApprovalGate` | Who must review non-auto-approved PRs |
| `config/dependency-rules.json` | Architectural isolation | 3 boundaries | `DependencyValidator` | Which cross-module includes are forbidden |
| `config/ownership-rules.json` | Team boundaries | 4 boundary definitions | `OwnershipValidator` | Which teams own which directories |

---

## Agent Integration Patterns

### Exit Code as Decision Signal

The control plane follows Unix convention: 0 = success, non-zero = action required. CI pipelines and agent scripts use simple `if`/`else` on exit codes:

```sh
./build/ai-governance-platform check-approval
if [ $? -eq 0 ]; then
  echo "Auto-approved — safe to merge"
else
  echo "Requires human review"
fi
```

### Trace/Audit Feedback Loop

After execution, agents can read `audit_*.json` to understand *why* a policy fired — which file, which rule, which threshold was exceeded. This enables self-diagnosis and adjustment of future behavior.

### Config-Driven Governance

All policies are JSON files. Agents can modify any config file to adjust enforcement behavior without recompiling the binary. This decouples governance logic (C++ binary) from policy definition (JSON files).

### Single-Binary Deployment

The compiled binary (`ai-governance-platform`) and its config files are the only deployable units. Zero runtime dependencies. This means any agent environment — local dev machine, CI runner, container — can enforce the same policies by receiving the same binary and configs.

---

## Quick Reference

```
CLI:  ./ai-governance-platform <command>
C++:  C++20, CMake 3.20+
Dep:  nlohmann/json v3.11.2 (FetchContent, no package manager)
LOC:  1,852 lines across 11 source + 10 header files
CI:   GitHub Actions — 12-step workflow with timeout safety on every command
```

Build instructions, command details, and development notes are in `AGENTS.md`.
