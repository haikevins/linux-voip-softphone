#!/usr/bin/env bash
# Reproduce the concurrency core, MPMC stress, and ThreadSanitizer gates.
set -euo pipefail

ROOT="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/../.." && pwd -P)"
MODE="${1:-all}"
export PYTHONDONTWRITEBYTECODE=1

case "$MODE" in
    core|stress|tsan|all) ;;
    *)
        printf 'Usage: bash %s [core|stress|tsan|all]\n' "$0" >&2
        exit 2
        ;;
esac

require() {
    command -v "$1" >/dev/null 2>&1 || {
        printf 'ERROR: %s is required\n' "$1" >&2
        exit 1
    }
}

for tool in cmake ctest; do
    require "$tool"
done

TSAN_TARGETS=(
    thread_safe_queue_tests
    serial_task_worker_tests
    thread_ownership_tests
    mpmc_stress_tests
    pjsip_call_slot_cleanup_policy_tests
    pjsip_callback_activity_tests
    pjsip_lookup_to_gate_lifetime_model_tests
)

verify_core() {
    local build_dir="${THREADING_BUILD_DIR:-$ROOT/build/threading-debug}"
    cmake -S "$ROOT" -B "$build_dir" -DCMAKE_BUILD_TYPE=Debug
    cmake --build "$build_dir" --target "${TSAN_TARGETS[@]}" --parallel 2
    ctest --test-dir "$build_dir" --output-on-failure -L '^tsan$'
}

verify_stress() {
    local build_dir="${THREADING_BUILD_DIR:-$ROOT/build/threading-debug}"
    local repeats="${THREADING_STRESS_REPEATS:-30}"

    [[ "$repeats" =~ ^[0-9]+$ ]] && (( repeats >= 1 && repeats <= 100 )) || {
        echo 'ERROR: THREADING_STRESS_REPEATS must be an integer from 1 to 100' >&2
        exit 1
    }

    cmake -S "$ROOT" -B "$build_dir" -DCMAKE_BUILD_TYPE=Debug
    cmake --build "$build_dir" --target mpmc_stress_tests --parallel 2
    ctest --test-dir "$build_dir" --output-on-failure \
        -R '^MpmcStressTest$' --repeat "until-fail:$repeats"
}

verify_tsan() {
    local build_dir="${THREADING_TSAN_BUILD_DIR:-$ROOT/build/tsan-debug}"
    local repeats="${THREADING_TSAN_REPEATS:-1}"
    local log

    [[ "$repeats" =~ ^[0-9]+$ ]] && (( repeats >= 1 && repeats <= 10 )) || {
        echo 'ERROR: THREADING_TSAN_REPEATS must be an integer from 1 to 10' >&2
        exit 1
    }

    cmake -S "$ROOT" -B "$build_dir" \
        -DCMAKE_BUILD_TYPE=Debug \
        -DENABLE_TSAN=ON
    cmake --build "$build_dir" --target "${TSAN_TARGETS[@]}" --parallel 2

    log="$build_dir/ctest-tsan.log"
    if TSAN_OPTIONS="${TSAN_OPTIONS:+$TSAN_OPTIONS:}halt_on_error=1:exitcode=66" \
        ctest --test-dir "$build_dir" --output-on-failure \
            -L '^tsan$' --repeat "until-fail:$repeats" 2>&1 | tee "$log"; then
        echo "PASS: ThreadSanitizer gate (${#TSAN_TARGETS[@]} tests x $repeats run(s))"
    else
        if grep -Eq 'unexpected memory mapping|failed to mmap' "$log"; then
            echo 'BLOCKED: TSan runtime incompatible with this execution environment.' >&2
        elif grep -Eq 'WARNING: ThreadSanitizer|ThreadSanitizer: data race' "$log"; then
            echo 'FAIL: ThreadSanitizer reported a possible race.' >&2
        else
            echo 'FAIL: TSan build/test failed; inspect the log.' >&2
        fi
        exit 1
    fi
}

case "$MODE" in
    core) verify_core ;;
    stress) verify_stress ;;
    tsan) verify_tsan ;;
    all)
        verify_core
        verify_stress
        verify_tsan
        ;;
esac
