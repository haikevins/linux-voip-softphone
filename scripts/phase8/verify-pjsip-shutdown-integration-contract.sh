#!/usr/bin/env bash
set -euo pipefail

# Audit passes only when both production stop() sites are still identified as
# UNSAFE/BLOCKED. Do not interpret this gate as live PJSUA2 shutdown safety.
ROOT="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/../.." && pwd -P)"
FOCUSED="${PHASE8_BUILD_DIR:-$ROOT/build/phase8-queue-debug}"
CANONICAL="${PHASE8_CANONICAL_BUILD_DIR:-$ROOT/build/qt-pjsip-gui-debug}"

for tool in python3 cmake ctest; do
    command -v "$tool" >/dev/null 2>&1 || {
        printf 'ABORT: missing tool: %s\n' "$tool" >&2
        exit 1
    }
done
[[ "$FOCUSED" != "$CANONICAL" ]] || {
    printf '%s\n' 'ABORT: focused and canonical build dirs must differ' >&2
    exit 1
}
export PYTHONDONTWRITEBYTECODE=1
cd "$ROOT"

echo '===== REAL PRODUCTION SHUTDOWN INTEGRATION BLOCKER INVENTORY ====='
python3 -B scripts/phase8/audit-pjsip-shutdown-integration.py \
    --source-root "$ROOT"

echo '===== VERIFY READINESS FAILS CLOSED ====='
if python3 -B scripts/phase8/audit-pjsip-shutdown-integration.py \
    --source-root "$ROOT" --require-ready >/dev/null; then
    printf '%s\n' 'ABORT: audit incorrectly declared real teardown READY' >&2
    exit 1
else
    status=$?
    [[ "$status" -eq 3 ]] || {
        printf 'ABORT: unexpected readiness status: %s\n' "$status" >&2
        exit 1
    }
fi
printf '%s\n' 'PASS: actual PJSUA2 integration remains explicitly BLOCKED'

echo '===== COMMIT 19 FOCUSED STATIC AUDIT CTEST ====='
cmake -S tests/phase8 -B "$FOCUSED" -DCMAKE_BUILD_TYPE=Debug
ctest --test-dir "$FOCUSED" --output-on-failure \
    -R '^Phase8PjsipShutdownIntegrationContractTest$'

echo '===== COMMIT 19 CANONICAL STATIC AUDIT CTEST ====='
[[ -f "$CANONICAL/CMakeCache.txt" ]] || {
    printf 'ABORT: canonical build not configured: %s\n' "$CANONICAL" >&2
    exit 1
}
cmake -S . -B "$CANONICAL"
ctest --test-dir "$CANONICAL" --output-on-failure \
    -R '^Phase8PjsipShutdownIntegrationContractTest$'

printf '%s\n' 'PASS: Phase 8 PJSIP shutdown integration BLOCKER audit gate (live PJSUA2 NOT TESTED)'
