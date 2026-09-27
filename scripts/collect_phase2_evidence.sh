#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
EVIDENCE_DIR="${ROOT_DIR}/evidence/02-linux"
mkdir -p "${EVIDENCE_DIR}"

# Remove only Phase 2 generated artifacts so a fresh collection cannot silently
# retain stale logs from an older revision.
rm -f \
    "${EVIDENCE_DIR}/README.md" \
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
    "${EVIDENCE_DIR}/format-check.log" \
    "${EVIDENCE_DIR}/git-history.txt" \
    "${EVIDENCE_DIR}/git-status.txt"

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
    git log --oneline --decorate -30
) 2>&1 | tee "${EVIDENCE_DIR}/git-history.txt"

(
    cd "${ROOT_DIR}"
    git status --short --branch
) 2>&1 | tee "${EVIDENCE_DIR}/git-status.txt"

cat > "${EVIDENCE_DIR}/README.md" <<EOF_README
# Phase 2 evidence — Linux system programming and IPC

Collected at: ${COLLECTED_AT}

Branch: \`${BRANCH:-unknown}\`
HEAD at collection time: \`${HEAD_SHA:-unknown}\`

This directory is the reproducible evidence snapshot for Phase 2. The collector
exits immediately if a build, test, sanitizer, clang-tidy, or clang-format gate
fails.

## Evidence files

- \`toolchain.txt\` — compiler, CMake, Git, Clang tooling, collection timestamp.
- \`build.log\` — Debug configure/build output.
- \`unit-tests.log\` — GoogleTest unit suite, including UniqueFd and Unix socket unit tests.
- \`integration-tests.log\` — Unix Domain Socket client/server end-to-end tests.
- \`all-tests.log\` — complete CTest regression suite.
- \`process-lifecycle-demo.log\` — fork/waitpid process lifecycle self-test.
- \`exec-child-demo.log\` — fork/exec process-image replacement self-test.
- \`signal-handling-demo.log\` — sigaction/sigsuspend graceful signal self-test.
- \`sanitizers.log\` — clean ASan + UBSan rebuild and test run.
- \`clang-tidy.log\` — clean clang-tidy rebuild.
- \`format-check.log\` — clang-format gate.
- \`git-history.txt\` — commit history visible at collection time.
- \`git-status.txt\` — working-tree state at collection time.

## Phase 2 capabilities represented

- RAII ownership for Linux file descriptors.
- \`fork()\`, \`execv()\`, \`waitpid()\`, PID/process lifecycle.
- POSIX signal handling with \`sigaction()\` and race-aware waiting via \`sigsuspend()\`.
- Unix Domain Socket server/client lifecycle using \`AF_UNIX\` and \`SOCK_STREAM\`.
- Client/server IPC round-trip and sequential-connection integration tests.

The final evidence commit itself is expected not to appear in
\`git-history.txt\`, because this snapshot must be generated before that commit
exists.
EOF_README

echo "Evidence written to: ${EVIDENCE_DIR}"
