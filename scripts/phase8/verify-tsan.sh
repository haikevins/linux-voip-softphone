#!/usr/bin/env bash
set -euo pipefail

# Build only the standalone C++20 concurrency test suite with TSan.
# Never reuse the canonical Qt/PJSIP, ASan/UBSan, or non-instrumented tree.
REPO_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
SOURCE_DIR="$REPO_ROOT/tests/phase8"
BUILD_DIR="${PHASE8_TSAN_BUILD_DIR:-$REPO_ROOT/build/phase8-tsan-debug}"
REPEATS="${PHASE8_TSAN_REPEATS:-1}"

for command_name in cmake ctest realpath; do
    command -v "$command_name" >/dev/null || {
        printf 'ERROR: %s is required\n' "$command_name" >&2
        exit 1
    }
done

case "$REPEATS" in
    ''|*[!0-9]*)
        echo 'ERROR: PHASE8_TSAN_REPEATS must be an integer from 1 to 10' >&2
        exit 1
        ;;
esac
if (( REPEATS < 1 || REPEATS > 10 )); then
    echo 'ERROR: PHASE8_TSAN_REPEATS must be an integer from 1 to 10' >&2
    exit 1
fi

# Compare canonicalized paths; refuse to reconfigure any existing standard
# project/test build tree with a different sanitizer configuration.
BUILD_DIR="$(realpath -m -- "$BUILD_DIR")"
case "$BUILD_DIR" in
    "$REPO_ROOT"|"$REPO_ROOT/build/qt-pjsip-gui-debug"|\
    "$REPO_ROOT/build/phase8-queue-debug"|\
    "$REPO_ROOT/build/phase8-stress-debug"|"$SOURCE_DIR")
        echo "ERROR: unsafe/reused build directory: $BUILD_DIR" >&2
        exit 1
        ;;
esac

test -f "$SOURCE_DIR/CMakeLists.txt"
test -f "$SOURCE_DIR/test_mpmc_stress.cpp"
test -f "$REPO_ROOT/src/concurrency/ThreadSafeQueue.hpp"
test -f "$REPO_ROOT/src/concurrency/SerialTaskWorker.hpp"

printf 'Phase 8 standalone ThreadSanitizer gate\nSource: %s\nBuild:  %s\nRepeats: %s\n' \
    "$SOURCE_DIR" "$BUILD_DIR" "$REPEATS"
cmake -S "$SOURCE_DIR" -B "$BUILD_DIR" \
    -DCMAKE_BUILD_TYPE=Debug -DPHASE8_ENABLE_TSAN=ON
cmake --build "$BUILD_DIR" --parallel 2

# TSan reports must cause a failing gate. Keep the CTest transcript in a
# generated build directory (NOT a repository evidence file).
LOG="$BUILD_DIR/ctest-tsan.log"
if TSAN_OPTIONS="${TSAN_OPTIONS:+$TSAN_OPTIONS:}halt_on_error=1:exitcode=66" \
    ctest --test-dir "$BUILD_DIR" --output-on-failure \
    --repeat "until-fail:$REPEATS" 2>&1 | tee "$LOG"; then
    echo "PASS: Phase 8 TSan gate (five tests x $REPEATS run(s))"
else
    if grep -Eq 'unexpected memory mapping|failed to mmap' "$LOG"; then
        echo 'BLOCKED: TSan runtime incompatible with this execution environment.' >&2
    elif grep -Eq 'WARNING: ThreadSanitizer|ThreadSanitizer: data race' "$LOG"; then
        echo 'FAIL: ThreadSanitizer reported a possible race.' >&2
    else
        echo 'FAIL: TSan build ran, but at least one test failed; see log.' >&2
    fi
    echo "TSan test log: $LOG" >&2
    exit 1
fi
