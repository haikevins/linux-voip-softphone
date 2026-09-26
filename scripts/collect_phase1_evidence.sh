#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
EVIDENCE_DIR="${ROOT_DIR}/evidence/01-cpp"
mkdir -p "${EVIDENCE_DIR}"

{
    echo "Collected at: $(date --iso-8601=seconds)"
    echo
    git --version
    cmake --version | head -n 1
    c++ --version | head -n 1
    clang --version | head -n 1 || true
    clang-format --version || true
    clang-tidy --version | head -n 1 || true
} | tee "${EVIDENCE_DIR}/toolchain.txt"

(
    cd "${ROOT_DIR}"
    cmake --preset debug
    cmake --build --preset debug -j
) 2>&1 | tee "${EVIDENCE_DIR}/build.log"

(
    cd "${ROOT_DIR}"
    ctest --preset debug
) 2>&1 | tee "${EVIDENCE_DIR}/unit-tests.log"

(
    cd "${ROOT_DIR}"
    ./build/debug/labs/cpp/raii/lab_raii
) 2>&1 | tee "${EVIDENCE_DIR}/raii-demo.log"

(
    cd "${ROOT_DIR}"
    ./build/debug/labs/cpp/smart-pointers/lab_smart_pointers
) 2>&1 | tee "${EVIDENCE_DIR}/smart-pointers-demo.log"

(
    cd "${ROOT_DIR}"
    ./build/debug/labs/cpp/move-semantics/lab_move_semantics
) 2>&1 | tee "${EVIDENCE_DIR}/move-semantics-demo.log"

(
    cd "${ROOT_DIR}"
    cmake --preset sanitizers
    cmake --build --preset sanitizers -j
    ctest --preset sanitizers
) 2>&1 | tee "${EVIDENCE_DIR}/sanitizers.log"

(
    cd "${ROOT_DIR}"
    ./scripts/run_clang_tidy.sh
) 2>&1 | tee "${EVIDENCE_DIR}/clang-tidy.log"

(
    cd "${ROOT_DIR}"
    find src tests labs -type f \( -name '*.cpp' -o -name '*.hpp' \) \
        -print0 | xargs -0 clang-format --dry-run --Werror
    echo "clang-format: PASS"
) 2>&1 | tee "${EVIDENCE_DIR}/format-check.log"

(
    cd "${ROOT_DIR}"
    if git rev-parse --is-inside-work-tree >/dev/null 2>&1; then
        git log --oneline --decorate --graph -20
    else
        echo "Not collected: package directory is not a Git working tree."
    fi
) 2>&1 | tee "${EVIDENCE_DIR}/git-history.txt"

echo "Evidence written to: ${EVIDENCE_DIR}"
