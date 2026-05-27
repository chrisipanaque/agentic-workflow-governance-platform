#!/bin/bash
set -euo pipefail

REPO_URL="https://github.com/chrisipanaque/agentic-workflow-governance-tools"
SUBMODULE_DIR="_gov"
BINARY_NAME="agentic-workflow-governance-tools"

GREEN='\033[0;32m'
RED='\033[0;31m'
NC='\033[0m'

command -v cmake >/dev/null 2>&1 || { echo -e "${RED}Error: cmake is required but not installed.${NC}"; exit 1; }
command -v git >/dev/null 2>&1 || { echo -e "${RED}Error: git is required but not installed.${NC}"; exit 1; }
if ! command -v g++ >/dev/null 2>&1 && ! command -v clang++ >/dev/null 2>&1; then
    echo -e "${RED}Error: C++ compiler (g++ or clang++) is required but not installed.${NC}"
    exit 1
fi

if [ ! -d ".git" ] && ! git rev-parse --git-dir >/dev/null 2>&1; then
    echo -e "${RED}Error: not inside a git repository. Run this script from your repo root.${NC}"
    exit 1
fi

if [ -d "$SUBMODULE_DIR" ]; then
    echo "Submodule '$SUBMODULE_DIR' already exists. Updating..."
    git submodule update --init --recursive "$SUBMODULE_DIR"
else
    echo "Adding governance tool as a git submodule..."
    git submodule add "$REPO_URL" "$SUBMODULE_DIR"
    if [ -n "${CI:-}" ]; then
        git config --file .gitmodules submodule."$SUBMODULE_DIR".shallow true
        git add .gitmodules
    fi
fi

echo "Building governance tool..."
cmake -B "$SUBMODULE_DIR/build" -DCMAKE_BUILD_TYPE=Release
cmake --build "$SUBMODULE_DIR/build"

if [ ! -d "config" ]; then
    echo "Copying default config files to your repo's config/..."
    cp -r "$SUBMODULE_DIR/config" .
    echo "Edit these files to set your own policies: config/forbidden-paths.json config/dependency-rules.json"
fi

echo "Adding $SUBMODULE_DIR/build/ to .gitignore..."
if [ -f ".gitignore" ]; then
    if ! grep -qxF "$SUBMODULE_DIR/build/" .gitignore; then
        echo "$SUBMODULE_DIR/build/" >> .gitignore
    fi
else
    echo "$SUBMODULE_DIR/build/" > .gitignore
fi

mkdir -p .github/workflows

CI_FILE=".github/workflows/governance.yml"
if [ -f "$CI_FILE" ]; then
    echo "CI workflow already exists at $CI_FILE. Skipping."
else
    echo "Writing CI workflow to $CI_FILE..."
    cat > "$CI_FILE" << 'YAML'
name: Governance Check
on:
  pull_request:
    branches: [main]
jobs:
  validate:
    runs-on: ubuntu-latest
    steps:
      - uses: actions/checkout@v4
        with:
          submodules: recursive
      - name: Install dependencies
        run: sudo apt-get update && sudo apt-get install -y cmake build-essential
      - name: Build governance tool
        run: |
          cmake -B _gov/build -DCMAKE_BUILD_TYPE=Release
          cmake --build _gov/build
      - name: Run policy validation
        run: ./_gov/build/agentic-workflow-governance-tools validate-policy
      - name: Upload reports
        if: always()
        uses: actions/upload-artifact@v4
        with:
          name: governance-reports
          path: output/
YAML
fi

echo ""
echo -e "${GREEN}Setup complete!${NC}"
echo ""
echo "  Run locally:     ./$SUBMODULE_DIR/build/$BINARY_NAME validate-policy"
echo "  Run in CI:       git push (workflow at $CI_FILE will run)"
echo "  All commands:    ./$SUBMODULE_DIR/build/$BINARY_NAME <command>"
echo "  Build dir:       $SUBMODULE_DIR/build/ (gitignored)"
echo ""
echo "Next steps:"
echo "  1. cd $SUBMODULE_DIR && git config user.email ... (if needed for testing)"
echo "  2. Make some changes in your repo"
echo "  3. Run: ./$SUBMODULE_DIR/build/$BINARY_NAME validate-policy"
echo ""
echo "To update the governance tool later:"
echo "  cd $SUBMODULE_DIR && git pull origin main && cd .. && cmake -B $SUBMODULE_DIR/build && cmake --build $SUBMODULE_DIR/build"
