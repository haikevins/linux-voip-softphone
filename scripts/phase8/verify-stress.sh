#!/usr/bin/env bash
set -euo pipefail

REPO_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
SOURCE_DIR="$REPO_ROOT/tests/phase8"
BUILD_DIR="${PHASE8_STRESS_BUILD_DIR:-$REPO_ROOT/build/phase8-stress-debug}"
REPEATS="${PHASE8_STRESS_REPEATS:-10}"

command -v cmake >/dev/null || { echo 'ERROR: cmake is required' >&2; exit 1; }
command -v ctest >/dev/null || { echo 'ERROR: ctest is required' >&2; exit 1; }
test -f "$SOURCE_DIR/test_mpmc_stress.cpp"

case "$REPEATS" in
    ''|*[!0-9]*)
        echo 'ERROR: PHASE8_STRESS_REPEATS must be an integer from 1 to 100' >&2
        exit 1
        ;;
esac
if (( REPEATS < 1 || REPEATS > 100 )); then
    echo 'ERROR: PHASE8_STRESS_REPEATS must be an integer from 1 to 100' >&2
    exit 1
fi

printf 'Phase 8 reproducible MPMC stress gate\nBuild: %s\nRuns: %s\n' "$BUILD_DIR" "$REPEATS"
cmake -S "$SOURCE_DIR" -B "$BUILD_DIR" -DCMAKE_BUILD_TYPE=Debug
cmake --build "$BUILD_DIR" --target phase8_mpmc_stress_tests --parallel 2
ctest --test-dir "$BUILD_DIR" --output-on-failure \
    -R '^Phase8MpmcStressTest$' --repeat "until-fail:$REPEATS"
