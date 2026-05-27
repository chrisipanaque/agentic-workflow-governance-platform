# agentic-workflow-governance-tools

C++20 CLI tool that scans git diffs, validates paths against policies, and computes risk scores for PR review safety gating.

## Quick Start

Run this inside your repo to set up the governance tool as a git submodule, build it, and create a CI workflow:

```sh
curl -fsSL https://raw.githubusercontent.com/chrisipanaque/agentic-workflow-governance-tools/main/setup.sh | bash
```

After setup, test it locally:

```sh
./_gov/build/agentic-workflow-governance-tools validate-policy
```

## Build & Run

```sh
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build
./build/agentic-workflow-governance-tools <command>
```

Single dependency: `nlohmann_json` (v3.11.2, fetched via CMake FetchContent). No package manager required.

## Commands

| Command | What it does | Exit 0 |
|---|---|---|
| `healthcheck` | Prints status, writes audit/trace logs | always |
| `show-policies` | Loads `config/forbidden-paths.json`, prints count | always |
| `scan-diff` | Runs `git diff --unified=0` via popen, prints stats | always |
| `validate-policy` | scan-diff + check paths against policy | if no violations |
| `risk-score` | scan-diff + score against `config/risk-rules.json` | if score < 75 |
| `check-approval` | risk-score + approval gating against configurable thresholds | if auto-approved (score < require_review) |
| `validate-architecture` | scan-diff + dependency boundary & ownership validation | if no dependency violations |

All commands write JSON audit reports to `output/reports/audit_*.json` and JSONL traces to `output/traces/trace_*.jsonl`.

## Architecture

- `src/main.cpp` — CLI dispatcher (7 commands, string comparison)
- Config files in `config/`: `forbidden-paths.json` (path + reason), `risk-rules.json` (pattern + weight)
- `src/diff_scanner.cpp` — parses `git diff --unified=0` output, not `git diff --cached` (unstaged+staged, not just staged)
- `src/path_validator.cpp` — glob matching (`*`/`?`) against forbidden paths
- `src/risk_engine.cpp` — weight-based scoring, capped at 100; thresholds: ≥75 CRITICAL, ≥50 HIGH, ≥25 MEDIUM
- `src/github_metadata.cpp` — reads `GITHUB_*` env vars (empty when run locally)
- `src/trace_logger.cpp` / `src/audit_log.cpp` — creates dirs via `std::filesystem::create_directories()` (recursive, no shell dependency), flushes on every command

## Testing

No unit test framework. CI (`.github/workflows/pr-governance-check.yml`) runs each command as a smoke test. For `scan-diff`, CI initializes a temp git repo with changes to avoid depending on the repo's actual diff.

To test locally, ensure you have staged/unstaged changes in a git repo, then run any command.

## Notable

- No linter, formatter, or typechecker config in the repo
- `build/`, `output/`, and `var/` are gitignored
- Exit code 1 also means "unknown command"
- `risk-score` returns 1 when score ≥ 75 (CRITICAL)
- `validate-policy` returns 1 when violations found

## Gotchas

- **Never iterate `github_metadata.to_json().items()` directly.** The temporary `json` is destroyed before the loop body, a dangling-pointer UB. Always store in a named variable first: `auto m = github_metadata.to_json(); for (const auto& kv : m.items())`
- `diff_scanner` uses `git --no-pager diff --unified=0` via `popen()` to prevent pager-triggered blocking
- All output to stdout uses `std::cout`; diagnostic trace markers use `std::cerr` (unbuffered, visible immediately in CI)
- CI step for `show-policies` wraps the binary in `timeout 30` as a safety net
