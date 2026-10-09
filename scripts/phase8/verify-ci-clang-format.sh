#!/usr/bin/env bash
set -euo pipefail

# Exact source set and clang-format arguments used by
# .github/workflows/static-analysis.yml (Static Analysis / Check formatting).
ROOT="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/../.." && pwd -P)"
cd "$ROOT"

FORMATTER="${CLANG_FORMAT_BIN:-clang-format}"
if ! command -v "$FORMATTER" >/dev/null 2>&1; then
    echo "ABORT: missing clang-format binary: $FORMATTER" >&2
    echo "Install the same clang-format used by Ubuntu 24.04 CI." >&2
    exit 1
fi

printf '===== CLANG FORMAT VERSION =====\n'
"$FORMATTER" --version
printf '===== REPRODUCE CI FORMAT GATE =====\n'
find src tests labs -type f \( -name '*.cpp' -o -name '*.hpp' \) -print0 \
    | xargs -0 "$FORMATTER" --dry-run --Werror
printf 'PASS: Phase 8 CI clang-format gate (src tests labs)\n'
