#!/usr/bin/env bash
# Consolidated Phase 8 queue/worker, stress and standalone TSan verification.
set -euo pipefail

ROOT="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/../.." && pwd -P)"
SOURCE_DIR="$ROOT/tests/phase8"
MODE="${1:-all}"

case "$MODE" in
    queue|stress|tsan|all) ;;
    *)
        printf 'Usage: bash %s [queue|stress|tsan|all]\n' "$0" >&2
        exit 2
        ;;
esac

verify_queue() {
    local build_dir="${PHASE8_BUILD_DIR:-$ROOT/build/phase8-queue-debug}"
    command -v cmake >/dev/null || { echo 'ERROR: cmake is required' >&2; exit 1; }
    command -v ctest >/dev/null || { echo 'ERROR: ctest is required' >&2; exit 1; }

    test -f "$SOURCE_DIR/CMakeLists.txt"
    test -f "$ROOT/src/concurrency/ThreadSafeQueue.hpp"
    test -f "$ROOT/src/concurrency/SerialTaskWorker.hpp"
    test -f "$SOURCE_DIR/test_thread_ownership.cpp"
    test -f "$SOURCE_DIR/test_mpmc_stress.cpp"
    test -f "$SOURCE_DIR/test_pjsip_call_slot_cleanup.cpp"
    test -f "$SOURCE_DIR/test_pjsip_callback_activity.cpp"
    test -f "$SOURCE_DIR/test_pjsip_production_lifetime_audit.py"

    printf 'Phase 8 queue, worker, ownership MPMC stress and PJSIP slot/callback activity and production source-audit verification\nSource: %s\nBuild:  %s\n' \
        "$SOURCE_DIR" "$build_dir"
    cmake -S "$SOURCE_DIR" -B "$build_dir" -DCMAKE_BUILD_TYPE=Debug
    cmake --build "$build_dir" --parallel 2
    ctest --test-dir "$build_dir" --output-on-failure
}

verify_stress() {
    local build_dir="${PHASE8_STRESS_BUILD_DIR:-$ROOT/build/phase8-stress-debug}"
    local repeats="${PHASE8_STRESS_REPEATS:-10}"

    command -v cmake >/dev/null || { echo 'ERROR: cmake is required' >&2; exit 1; }
    command -v ctest >/dev/null || { echo 'ERROR: ctest is required' >&2; exit 1; }
    test -f "$SOURCE_DIR/test_mpmc_stress.cpp"

    case "$repeats" in
        ''|*[!0-9]*)
            echo 'ERROR: PHASE8_STRESS_REPEATS must be an integer from 1 to 100' >&2
            exit 1
            ;;
    esac
    if (( repeats < 1 || repeats > 100 )); then
        echo 'ERROR: PHASE8_STRESS_REPEATS must be an integer from 1 to 100' >&2
        exit 1
    fi

    printf 'Phase 8 reproducible MPMC stress gate\nBuild: %s\nRuns: %s\n' "$build_dir" "$repeats"
    cmake -S "$SOURCE_DIR" -B "$build_dir" -DCMAKE_BUILD_TYPE=Debug
    cmake --build "$build_dir" --target phase8_mpmc_stress_tests --parallel 2
    ctest --test-dir "$build_dir" --output-on-failure \
        -R '^Phase8MpmcStressTest$' --repeat "until-fail:$repeats"
}

verify_tsan() {
    local build_dir="${PHASE8_TSAN_BUILD_DIR:-$ROOT/build/phase8-tsan-debug}"
    local repeats="${PHASE8_TSAN_REPEATS:-1}"
    local command_name log

    for command_name in cmake ctest realpath; do
        command -v "$command_name" >/dev/null || {
            printf 'ERROR: %s is required\n' "$command_name" >&2
            exit 1
        }
    done

    case "$repeats" in
        ''|*[!0-9]*)
            echo 'ERROR: PHASE8_TSAN_REPEATS must be an integer from 1 to 10' >&2
            exit 1
            ;;
    esac
    if (( repeats < 1 || repeats > 10 )); then
        echo 'ERROR: PHASE8_TSAN_REPEATS must be an integer from 1 to 10' >&2
        exit 1
    fi

    build_dir="$(realpath -m -- "$build_dir")"
    case "$build_dir" in
        "$ROOT"|"$ROOT/build/qt-pjsip-gui-debug"|\
        "$ROOT/build/phase8-queue-debug"|\
        "$ROOT/build/phase8-stress-debug"|"$SOURCE_DIR")
            echo "ERROR: unsafe/reused build directory: $build_dir" >&2
            exit 1
            ;;
    esac

    test -f "$SOURCE_DIR/CMakeLists.txt"
    test -f "$SOURCE_DIR/test_mpmc_stress.cpp"
    test -f "$ROOT/src/concurrency/ThreadSafeQueue.hpp"
    test -f "$ROOT/src/concurrency/SerialTaskWorker.hpp"

    printf 'Phase 8 standalone ThreadSanitizer gate\nSource: %s\nBuild:  %s\nRepeats: %s\n' \
        "$SOURCE_DIR" "$build_dir" "$repeats"
    cmake -S "$SOURCE_DIR" -B "$build_dir" \
        -DCMAKE_BUILD_TYPE=Debug -DPHASE8_ENABLE_TSAN=ON
    cmake --build "$build_dir" --parallel 2

    log="$build_dir/ctest-tsan.log"
    if TSAN_OPTIONS="${TSAN_OPTIONS:+$TSAN_OPTIONS:}halt_on_error=1:exitcode=66" \
        ctest --test-dir "$build_dir" --output-on-failure \
        --repeat "until-fail:$repeats" 2>&1 | tee "$log"; then
        echo "PASS: Phase 8 TSan gate (six tests x $repeats run(s))"
    else
        if grep -Eq 'unexpected memory mapping|failed to mmap' "$log"; then
            echo 'BLOCKED: TSan runtime incompatible with this execution environment.' >&2
        elif grep -Eq 'WARNING: ThreadSanitizer|ThreadSanitizer: data race' "$log"; then
            echo 'FAIL: ThreadSanitizer reported a possible race.' >&2
        else
            echo 'FAIL: TSan build ran, but at least one test failed; see log.' >&2
        fi
        echo "TSan test log: $log" >&2
        exit 1
    fi
}

case "$MODE" in
    queue) verify_queue ;;
    stress) verify_stress ;;
    tsan) verify_tsan ;;
    all)
        verify_queue
        verify_stress
        verify_tsan
        ;;
esac
