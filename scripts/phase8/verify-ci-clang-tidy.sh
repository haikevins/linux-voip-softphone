#!/usr/bin/env bash
set -euo pipefail

# Reproduce the configure+build jobs from .github/workflows/static-analysis.yml.
# This script must never suppress clang-tidy warnings or change the preset.
ROOT="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/../.." && pwd -P)"
cd "$ROOT"

for tool in cmake ninja clang-tidy; do
    if ! command -v "$tool" >/dev/null 2>&1; then
        echo "ABORT: missing CI clang-tidy requirement: $tool" >&2
        exit 1
    fi
done

printf '===== CLANG-TIDY VERSION =====\n'
clang-tidy --version
printf '===== CI CLANG-TIDY CONFIGURE =====\n'
cmake --preset clang-tidy
printf '===== CI CLANG-TIDY BUILD =====\n'
cmake --build --preset clang-tidy -j 2
printf 'PASS: Phase 8 CI clang-tidy preset build\n'
