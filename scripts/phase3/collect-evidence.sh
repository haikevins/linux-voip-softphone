#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
EVIDENCE_DIR="${ROOT_DIR}/evidence/phase3"
NETWORK_TEST_REGEX='^(Ipv4EndpointTest|TcpClientSocketTest|TcpServerSocketTest|UdpSocketTest|TcpNetworkIntegrationTest|UdpNetworkIntegrationTest)\.'

mkdir -p "${EVIDENCE_DIR}"

# Remove only generated Phase 3 artifacts. README.md is maintained separately.
# Obsolete VCS-bookkeeping artifacts are removed if an older collector left them.
rm -f \
    "${EVIDENCE_DIR}/toolchain.txt" \
    "${EVIDENCE_DIR}/build.log" \
    "${EVIDENCE_DIR}/network-tests.log" \
    "${EVIDENCE_DIR}/full-tests.log" \
    "${EVIDENCE_DIR}/sanitizers.log" \
    "${EVIDENCE_DIR}/clang-tidy.log" \
    "${EVIDENCE_DIR}/format-check.log" \
    "${EVIDENCE_DIR}/phase3-summary.txt"

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
    ./scripts/quality/sanitizers.sh
) 2>&1 | tee "${EVIDENCE_DIR}/sanitizers.log"

(
    cd "${ROOT_DIR}"
    rm -rf build/clang-tidy
    ./scripts/quality/clang-tidy.sh
) 2>&1 | tee "${EVIDENCE_DIR}/clang-tidy.log"

(
    cd "${ROOT_DIR}"
    find src tests labs -type f \( -name '*.cpp' -o -name '*.hpp' \) \
        -print0 | xargs -0 clang-format --dry-run --Werror
    echo "clang-format: PASS"
) 2>&1 | tee "${EVIDENCE_DIR}/format-check.log"

# Keep git diff --check as a quality gate, but do not store VCS bookkeeping as
# phase evidence.
(
    cd "${ROOT_DIR}"
    git diff --check
)

{
    echo "Phase 3: TCP/UDP networking"
    echo "Focused networking test target: IPv4 endpoint, TCP client/server, UDP socket, TCP/UDP integrations"
    echo "Expected current regression baseline before evidence commit: 60 tests"
    echo "Verification command: ./scripts/phase3/verify.sh"
    echo "Evidence collector: ./scripts/phase3/collect-evidence.sh"
} | tee "${EVIDENCE_DIR}/phase3-summary.txt"

find "${EVIDENCE_DIR}" -maxdepth 1 -type f \
    \( -name '*.log' -o -name '*.txt' \) \
    -exec sed -i 's/[[:space:]]\+$//' {} +

echo "Evidence written to: ${EVIDENCE_DIR}"
