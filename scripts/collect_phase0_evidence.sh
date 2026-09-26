#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
EVIDENCE_DIR="${ROOT_DIR}/evidence/00-phase0"
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
) 2>&1 | tee "${EVIDENCE_DIR}/test.log"

(
    cd "${ROOT_DIR}"
    cmake --preset sanitizers
    cmake --build --preset sanitizers -j
    ctest --preset sanitizers
) 2>&1 | tee "${EVIDENCE_DIR}/sanitizers.log"

(
    cd "${ROOT_DIR}"
    cmake --preset clang-tidy
    cmake --build --preset clang-tidy -j
) 2>&1 | tee "${EVIDENCE_DIR}/clang-tidy.log"

echo "Evidence written to: ${EVIDENCE_DIR}"
