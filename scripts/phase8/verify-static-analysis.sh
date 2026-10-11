#!/usr/bin/env bash
# Phase 8 local reproduction of GitHub Static Analysis gates.
set -euo pipefail

ROOT="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/../.." && pwd -P)"
cd "$ROOT"

MODE="${1:-all}"
case "$MODE" in
    format|tidy|all) ;;
    *)
        printf 'Usage: bash %s [format|tidy|all]\n' "$0" >&2
        exit 2
        ;;
esac

verify_format() {
    local formatter="${CLANG_FORMAT_BIN:-clang-format}"
    command -v "$formatter" >/dev/null 2>&1 || {
        printf 'ABORT: missing clang-format binary: %s\n' "$formatter" >&2
        exit 1
    }

    printf '===== CLANG FORMAT VERSION =====\n'
    "$formatter" --version
    printf '===== REPRODUCE CI FORMAT GATE =====\n'
    find src tests labs -type f \( -name '*.cpp' -o -name '*.hpp' \) -print0 \
        | xargs -0 "$formatter" --dry-run --Werror
    printf 'PASS: Phase 8 CI clang-format gate (src tests labs)\n'
}

verify_tidy() {
    local tool
    for tool in cmake ninja clang-tidy; do
        command -v "$tool" >/dev/null 2>&1 || {
            printf 'ABORT: missing CI clang-tidy requirement: %s\n' "$tool" >&2
            exit 1
        }
    done

    printf '===== CLANG-TIDY VERSION =====\n'
    clang-tidy --version
    printf '===== CI CLANG-TIDY CONFIGURE =====\n'
    cmake --preset clang-tidy
    printf '===== CI CLANG-TIDY BUILD =====\n'
    cmake --build --preset clang-tidy -j 2
    printf 'PASS: Phase 8 CI clang-tidy preset build\n'
}

case "$MODE" in
    format) verify_format ;;
    tidy) verify_tidy ;;
    all)
        verify_format
        verify_tidy
        ;;
esac
