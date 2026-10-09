#!/usr/bin/env bash
set -euo pipefail

REPO_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
SOURCE_DIR="$REPO_ROOT/tests/phase8"
CANONICAL_BUILD_DIR="${PHASE8_CANONICAL_BUILD_DIR:-$REPO_ROOT/build/qt-pjsip-gui-debug}"
FOCUSED_BUILD_DIR="${PHASE8_BUILD_DIR:-$REPO_ROOT/build/phase8-queue-debug}"

for command_name in cmake ctest; do
    command -v "$command_name" >/dev/null || {
        printf 'ERROR: %s is required\n' "$command_name" >&2
        exit 1
    }
done

test -f "$REPO_ROOT/src/pjsip/CallbackActivityGate.hpp"
test -f "$SOURCE_DIR/test_pjsip_callback_activity.cpp"

printf '===== PHASE 8 CALLBACK ACTIVITY (STANDALONE) =====\n'
cmake -S "$SOURCE_DIR" -B "$FOCUSED_BUILD_DIR" -DCMAKE_BUILD_TYPE=Debug
cmake --build "$FOCUSED_BUILD_DIR" --target phase8_pjsip_callback_activity_tests --parallel 2
ctest --test-dir "$FOCUSED_BUILD_DIR" \
    -R '^Phase8PjsipCallbackActivityTest$' --output-on-failure

printf '===== PHASE 8 CALLBACK ACTIVITY (CANONICAL) =====\n'
cmake -S "$REPO_ROOT" -B "$CANONICAL_BUILD_DIR"
cmake --build "$CANONICAL_BUILD_DIR" --target phase8_pjsip_callback_activity_tests --parallel 2
ctest --test-dir "$CANONICAL_BUILD_DIR" \
    -R '^Phase8PjsipCallbackActivityTest$' --output-on-failure

printf 'PASS: Phase 8 callback activity model gate (standalone + canonical)\n'
printf 'NOTE: no production SIP callback is instrumented by this test.\n'
