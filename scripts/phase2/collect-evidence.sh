#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
EVIDENCE_DIR="${ROOT_DIR}/evidence/phase2"
mkdir -p "${EVIDENCE_DIR}"

# Remove only generated Phase 2 artifacts so a fresh collection cannot silently
# retain stale output. README.md is maintained separately and follows the common
# evidence schema; the collector must not rewrite it.
rm -f \
    "${EVIDENCE_DIR}/toolchain.txt" \
    "${EVIDENCE_DIR}/build.log" \
    "${EVIDENCE_DIR}/unit-tests.log" \
    "${EVIDENCE_DIR}/integration-tests.log" \
    "${EVIDENCE_DIR}/all-tests.log" \
    "${EVIDENCE_DIR}/process-lifecycle-demo.log" \
    "${EVIDENCE_DIR}/exec-child-demo.log" \
    "${EVIDENCE_DIR}/signal-handling-demo.log" \
    "${EVIDENCE_DIR}/sanitizers.log" \
    "${EVIDENCE_DIR}/clang-tidy.log" \
    "${EVIDENCE_DIR}/format-check.log"

COLLECTED_AT="$(date --iso-8601=seconds)"
BRANCH="$(git -C "${ROOT_DIR}" branch --show-current 2>/dev/null || true)"
HEAD_SHA="$(git -C "${ROOT_DIR}" rev-parse --short HEAD 2>/dev/null || true)"

{
    echo "Collected at: ${COLLECTED_AT}"
    echo "Branch: ${BRANCH:-unknown}"
    echo "HEAD: ${HEAD_SHA:-unknown}"
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
    ./build/debug/tests/unit_tests --gtest_color=no
) 2>&1 | tee "${EVIDENCE_DIR}/unit-tests.log"

(
    cd "${ROOT_DIR}"
    ./build/debug/tests/ipc_integration_tests --gtest_color=no
) 2>&1 | tee "${EVIDENCE_DIR}/integration-tests.log"

(
    cd "${ROOT_DIR}"
    ctest --preset debug --output-on-failure
) 2>&1 | tee "${EVIDENCE_DIR}/all-tests.log"

(
    cd "${ROOT_DIR}"
    ./build/debug/labs/linux/process-lifecycle/lab_process_lifecycle
) 2>&1 | tee "${EVIDENCE_DIR}/process-lifecycle-demo.log"

(
    cd "${ROOT_DIR}"
    ./build/debug/labs/linux/exec-child/lab_exec_child --self-test
) 2>&1 | tee "${EVIDENCE_DIR}/exec-child-demo.log"

(
    cd "${ROOT_DIR}"
    ./build/debug/labs/linux/signal-handling/lab_signal_handling --self-test
) 2>&1 | tee "${EVIDENCE_DIR}/signal-handling-demo.log"

(
    cd "${ROOT_DIR}"
    cmake --preset sanitizers
    cmake --build --preset sanitizers --clean-first -j
    ctest --preset sanitizers --output-on-failure
) 2>&1 | tee "${EVIDENCE_DIR}/sanitizers.log"

(
    cd "${ROOT_DIR}"
    ./scripts/quality/clang-tidy.sh
) 2>&1 | tee "${EVIDENCE_DIR}/clang-tidy.log"

(
    cd "${ROOT_DIR}"
    find src tests labs -type f \( -name '*.cpp' -o -name '*.hpp' \) \
        -print0 | xargs -0 clang-format --dry-run --Werror
    echo "clang-format: PASS"
) 2>&1 | tee "${EVIDENCE_DIR}/format-check.log"

echo "Evidence written to: ${EVIDENCE_DIR}"
