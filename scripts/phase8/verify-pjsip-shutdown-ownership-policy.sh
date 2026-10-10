#!/usr/bin/env bash
set -euo pipefail

# Synthetic per-attempt ownership policy verification; this gate does NOT
# establish real PJSUA2 callback quiescence or fix the SIP clients.
ROOT="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/../.." && pwd -P)"
FOCUSED="${PHASE8_BUILD_DIR:-$ROOT/build/phase8-queue-debug}"
CANONICAL="${PHASE8_CANONICAL_BUILD_DIR:-$ROOT/build/qt-pjsip-gui-debug}"

for tool in cmake ctest; do
    if ! command -v "$tool" >/dev/null 2>&1; then
        printf 'ABORT: required tool missing: %s\n' "$tool" >&2
        exit 1
    fi
done

if [[ "$FOCUSED" == "$CANONICAL" ]]; then
    printf '%s\n' 'ABORT: focused and canonical build directories must differ' >&2
    exit 1
fi

export PYTHONDONTWRITEBYTECODE=1
cd "$ROOT"
test -f tests/phase8/test_pjsip_shutdown_ownership_policy.cpp
test -f src/pjsip/CallbackActivityGate.hpp

printf '===== COMMIT 17 FOCUSED SYNTHETIC POLICY TARGET =====\n'
cmake -S tests/phase8 -B "$FOCUSED" -DCMAKE_BUILD_TYPE=Debug
cmake --build "$FOCUSED" --target phase8_pjsip_shutdown_ownership_policy_tests -j 2
ctest --test-dir "$FOCUSED" --output-on-failure \
    -R '^Phase8PjsipShutdownOwnershipPolicyTest$'

printf '===== COMMIT 17 CANONICAL SYNTHETIC POLICY TARGET =====\n'
if [[ ! -f "$CANONICAL/CMakeCache.txt" ]]; then
    printf 'ABORT: canonical build directory is not configured: %s\n' "$CANONICAL" >&2
    exit 1
fi
cmake -S . -B "$CANONICAL"
cmake --build "$CANONICAL" --target phase8_pjsip_shutdown_ownership_policy_tests -j 2
ctest --test-dir "$CANONICAL" --output-on-failure \
    -R '^Phase8PjsipShutdownOwnershipPolicyTest$'

printf '%s\n' 'PASS: Phase 8 PJSIP shutdown ownership POLICY MODEL gate (live PJSUA2 NOT TESTED)'
