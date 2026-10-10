#!/usr/bin/env bash
set -euo pipefail

# Exercise the production-facing authorization header in both CTest builds.
# Neither this gate nor the header proves live external PJSUA2 quiescence.
ROOT="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/../.." && pwd -P)"
FOCUSED="${PHASE8_BUILD_DIR:-$ROOT/build/phase8-queue-debug}"
CANONICAL="${PHASE8_CANONICAL_BUILD_DIR:-$ROOT/build/qt-pjsip-gui-debug}"

for tool in cmake ctest; do
    if ! command -v "$tool" >/dev/null 2>&1; then
        printf 'ABORT: missing required tool: %s\n' "$tool" >&2
        exit 1
    fi
done
if [[ "$FOCUSED" == "$CANONICAL" ]]; then
    printf '%s\n' 'ABORT: focused and canonical build dirs must differ' >&2
    exit 1
fi

export PYTHONDONTWRITEBYTECODE=1
cd "$ROOT"
test -f src/pjsip/PjsipShutdownAuthorization.hpp
test -f tests/phase8/test_pjsip_shutdown_authorization.cpp

echo '===== COMMIT 18 PRODUCTION-HEADER AUTHORIZATION (FOCUSED) ====='
cmake -S tests/phase8 -B "$FOCUSED" -DCMAKE_BUILD_TYPE=Debug
cmake --build "$FOCUSED" --target phase8_pjsip_shutdown_authorization_tests -j 2
ctest --test-dir "$FOCUSED" --output-on-failure \
    -R '^Phase8PjsipShutdownAuthorizationTest$'

echo '===== COMMIT 18 PRODUCTION-HEADER AUTHORIZATION (CANONICAL) ====='
if [[ ! -f "$CANONICAL/CMakeCache.txt" ]]; then
    printf 'ABORT: canonical build directory not configured: %s\n' "$CANONICAL" >&2
    exit 1
fi
cmake -S . -B "$CANONICAL"
cmake --build "$CANONICAL" --target phase8_pjsip_shutdown_authorization_tests -j 2
ctest --test-dir "$CANONICAL" --output-on-failure \
    -R '^Phase8PjsipShutdownAuthorizationTest$'

printf '%s\n' 'PASS: Phase 8 PJSIP shutdown authorization HEADER gate (live PJSUA2 NOT TESTED)'
