#!/usr/bin/env bash
set -euo pipefail

ROOT="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/../.." && pwd -P)"
FOCUSED="${PHASE8_BUILD_DIR:-$ROOT/build/phase8-queue-debug}"
CANONICAL="${PHASE8_CANONICAL_BUILD_DIR:-$ROOT/build/qt-pjsip-gui-debug}"

for tool in cmake ctest; do
    command -v "$tool" >/dev/null 2>&1 || {
        printf 'ABORT: required tool missing: %s\n' "$tool" >&2
        exit 1
    }
done

test -f "$ROOT/src/pjsip/CallbackActivityGate.hpp"
test -f "$ROOT/tests/phase8/test_pjsip_callback_ingress_races.cpp"

cd "$ROOT"
export PYTHONDONTWRITEBYTECODE=1

printf '===== SYNTHETIC EXTERNAL CALLBACK INGRESS (FOCUSED) =====\n'
cmake -S tests/phase8 -B "$FOCUSED" -DCMAKE_BUILD_TYPE=Debug
cmake --build "$FOCUSED" --target phase8_pjsip_callback_ingress_races_tests -j 2
ctest --test-dir "$FOCUSED" --output-on-failure \
    -R '^Phase8PjsipCallbackIngressRacesTest$'

printf '===== SYNTHETIC EXTERNAL CALLBACK INGRESS (CANONICAL) =====\n'
if [[ ! -f "$CANONICAL/CMakeCache.txt" ]]; then
    printf 'ABORT: canonical build directory is not configured: %s\n' "$CANONICAL" >&2
    exit 1
fi
cmake -S . -B "$CANONICAL"
cmake --build "$CANONICAL" --target phase8_pjsip_callback_ingress_races_tests -j 2
ctest --test-dir "$CANONICAL" --output-on-failure \
    -R '^Phase8PjsipCallbackIngressRacesTest$'

printf '%s\n' 'PASS: Phase 8 synthetic PJSIP callback ingress race MODEL gate (live PJSUA2 NOT TESTED)'
