#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
EVIDENCE_DIR="${ROOT_DIR}/evidence/03-networking"
NETWORK_TEST_REGEX='^(Ipv4EndpointTest|TcpClientSocketTest|TcpServerSocketTest|UdpSocketTest|TcpNetworkIntegrationTest|UdpNetworkIntegrationTest)\.'

mkdir -p "${EVIDENCE_DIR}"

{
    echo "Collected at: $(date --iso-8601=seconds)"
    echo "Branch: $(git -C "${ROOT_DIR}" branch --show-current)"
    echo "Commit: $(git -C "${ROOT_DIR}" rev-parse HEAD)"
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
    ctest --preset debug -R "${NETWORK_TEST_REGEX}" --output-on-failure
) 2>&1 | tee "${EVIDENCE_DIR}/network-tests.log"

(
    cd "${ROOT_DIR}"
    ctest --preset debug --output-on-failure
) 2>&1 | tee "${EVIDENCE_DIR}/full-tests.log"

(
    cd "${ROOT_DIR}"
    ./scripts/run_sanitizers.sh
) 2>&1 | tee "${EVIDENCE_DIR}/sanitizers.log"

(
    cd "${ROOT_DIR}"
    rm -rf build/clang-tidy
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
    git diff --check
    echo "git diff --check: PASS"
) 2>&1 | tee "${EVIDENCE_DIR}/git-diff-check.log"

(
    cd "${ROOT_DIR}"
    git log --oneline --decorate --graph -25
) 2>&1 | tee "${EVIDENCE_DIR}/git-history.txt"

{
    echo "Phase 3: TCP/UDP networking"
    echo "Focused networking test target: IPv4 endpoint, TCP client/server, UDP socket, TCP/UDP integrations"
    echo "Expected current regression baseline before evidence commit: 60 tests"
    echo "Verification command: ./scripts/verify_phase3.sh"
    echo "Evidence collector: ./scripts/collect_phase3_evidence.sh"
} | tee "${EVIDENCE_DIR}/phase3-summary.txt"

find "${EVIDENCE_DIR}" -maxdepth 1 -type f \
    \( -name '*.log' -o -name '*.txt' \) \
    -exec sed -i 's/[[:space:]]\+$//' {} +

echo "Evidence written to: ${EVIDENCE_DIR}"
