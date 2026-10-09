#!/usr/bin/env bash
set -euo pipefail

# A deterministic failure-path MODEL gate + a bounded inventory of current
# production hazards. Neither part establishes live PJSUA2 callback quiescence.
ROOT="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/../.." && pwd -P)"
FOCUSED="${PHASE8_BUILD_DIR:-$ROOT/build/phase8-queue-debug}"
CANONICAL="${PHASE8_CANONICAL_BUILD_DIR:-$ROOT/build/qt-pjsip-gui-debug}"

for tool in python3 cmake ctest; do
    if ! command -v "$tool" >/dev/null 2>&1; then
        printf 'ABORT: required tool unavailable: %s\n' "$tool" >&2
        exit 1
    fi
done

if [[ "$FOCUSED" == "$CANONICAL" ]]; then
    printf 'ABORT: focused and canonical build directories must differ\n' >&2
    exit 1
fi

export PYTHONDONTWRITEBYTECODE=1
cd "$ROOT"

echo '===== BOUNDED PRODUCTION SOURCE RISK INVENTORY ====='
python3 -B - <<'PY'
import importlib.util
from pathlib import Path
import sys

root = Path.cwd()
audit_path = root / 'scripts/phase8/audit-pjsip-production-lifetime.py'
spec = importlib.util.spec_from_file_location('phase8_shutdown_inventory', audit_path)
if spec is None or spec.loader is None:
    raise SystemExit('ABORT: cannot load existing production source audit')
audit = importlib.util.module_from_spec(spec)
sys.modules[spec.name] = audit
spec.loader.exec_module(audit)

outgoing = (root / 'src/pjsip/SipOutgoingCallClient.cpp').read_text(encoding='utf-8')
incoming = (root / 'src/pjsip/SipIncomingCallClient.cpp').read_text(encoding='utf-8')
result = audit.audit_pair(outgoing, incoming)
if result['source_contract'] != 'PASS':
    raise SystemExit('ABORT: production source structure changed; re-audit before proceeding')

unresolved = {
    'CALL_SLOT_WAIT_RESULT_DISCARDED',
    'ACCOUNT_SHUTDOWN_RESULT_DISCARDED',
    'ACCOUNT_DESTROYED_AFTER_POSSIBLE_SHUTDOWN_FAILURE',
    'REGISTRATION_CALLBACK_NO_TEARDOWN_GATE',
    'NO_PRODUCTION_CALLBACK_ACTIVITY_GATE',
    'CALLBACKS_BORROW_OWNER_BY_REFERENCE',
}
for client, state in result['clients'].items():
    missing = unresolved.difference(state['risks'])
    if missing:
        raise SystemExit(
            f'ABORT: {client} known-risk inventory changed; review {sorted(missing)}'
        )
    print(f'{client}: six known shutdown/callback lifetime risks OPEN (not repaired)')

print('PASS: production failure-path risk inventory (known risks remain OPEN)')
print('Live PJSUA2 callback quiescence: NOT TESTED')
PY

echo '===== FOCUSED MODEL TARGET ====='
cmake -S tests/phase8 -B "$FOCUSED" -DCMAKE_BUILD_TYPE=Debug
cmake --build "$FOCUSED" --target phase8_pjsip_shutdown_failure_paths_tests -j 2
ctest --test-dir "$FOCUSED" --output-on-failure \
    -R '^Phase8PjsipShutdownFailurePathsTest$'

echo '===== CANONICAL MODEL TARGET ====='
if [[ ! -f "$CANONICAL/CMakeCache.txt" ]]; then
    printf 'ABORT: canonical build not configured: %s\n' "$CANONICAL" >&2
    exit 1
fi
cmake -S . -B "$CANONICAL"
cmake --build "$CANONICAL" --target phase8_pjsip_shutdown_failure_paths_tests -j 2
ctest --test-dir "$CANONICAL" --output-on-failure \
    -R '^Phase8PjsipShutdownFailurePathsTest$'

printf '%s\n' 'PASS: Phase 8 PJSIP shutdown failure-path MODEL gate (live PJSUA2 NOT TESTED)'
