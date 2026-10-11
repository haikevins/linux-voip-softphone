#!/usr/bin/env bash
# Aggregate Phase 8 offline verification. Live SIP remains explicit opt-in.
set -euo pipefail

ROOT="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/../.." && pwd -P)"
cd "$ROOT"
export PYTHONDONTWRITEBYTECODE=1

fail() {
    printf 'ERROR: %s\n' "$*" >&2
    exit 1
}

for tool in bash cmake ctest git python3; do
    command -v "$tool" >/dev/null 2>&1 || fail "required command not found: $tool"
done

initial_status="$(git status --porcelain=v1 --untracked-files=all)"

echo '== repository quality =='
git diff --check
git diff --cached --check
bash scripts/quality/verify-docs-structure.sh
bash scripts/quality/verify-evidence-structure.sh

echo '== canonical Qt/PJSIP build and CTest =='
cmake --preset qt-pjsip-gui-debug
cmake --build --preset qt-pjsip-gui-debug --parallel 2
ctest --preset qt-pjsip-gui-debug

echo '== production PJSIP shutdown-order source regression =='
python3 -B tests/integration/test_pjsip_shutdown_order.py

echo '== concurrency core =='
bash scripts/phase8/verify-threading.sh core

echo '== repeated MPMC stress =='
THREADING_STRESS_REPEATS="${THREADING_STRESS_REPEATS:-30}" \
    bash scripts/phase8/verify-threading.sh stress

echo '== fail-closed production shutdown boundary =='
python3 -B tests/integration/support/pjsip_shutdown_integration_audit.py \
    --source-root "$ROOT"

set +e
python3 -B tests/integration/support/pjsip_shutdown_integration_audit.py \
    --source-root "$ROOT" --require-ready >/dev/null
ready_rc=$?
set -e
[[ "$ready_rc" -eq 3 ]] || fail "shutdown readiness audit returned unexpected status: $ready_rc"

echo '== recorded controlled live lifecycle evidence =='
evidence='evidence/phase8/pjsip-live-lifecycle.txt'
[[ -s "$evidence" ]] || fail "missing $evidence"
for marker in \
    'FIRST_SHUTDOWN=PJ_EBUSY' \
    'SECOND_SHUTDOWN=SUCCESS' \
    'PROBE_RESULT=PASS' \
    'CONTROLLED_PJSUA2_PJ_EBUSY_RECOVERY=LIVE_OBSERVED_EVIDENCED' \
    'LIVE_EXTERNAL_CALLBACK_QUIESCENCE=UNPROVEN' \
    'PRODUCTION_INTEGRATION_READINESS=BLOCKED'
do
    grep -Fxq "$marker" "$evidence" || fail "missing evidence marker: $marker"
done

if grep -Ein \
    '^[[:space:]]*(Proxy-)?Authorization:[[:space:]]*Digest|nonce="|cnonce="|response="|a=crypto:.*inline:' \
    "$evidence" >/dev/null 2>&1; then
    fail 'secret-bearing SIP/SRTP material detected in live evidence'
fi

final_status="$(git status --porcelain=v1 --untracked-files=all)"
[[ "$final_status" == "$initial_status" ]] || fail 'verification modified the source checkout'

echo 'Phase 8 offline verification: PASS'
echo 'EVIDENCE: controlled PJSUA2 PJ_EBUSY recovery was observed in the recorded live probe.'
echo 'BOUNDARY: live external callback quiescence remains UNPROVEN.'
echo 'BOUNDARY: production-client real PJ_EBUSY recovery remains UNPROVEN.'
echo 'BOUNDARY: PJSUA2 integration readiness remains BLOCKED.'
