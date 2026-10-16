#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
cd "${ROOT_DIR}"

fail() {
    printf 'ERROR: %s\n' "$*" >&2
    exit 1
}

require_command() {
    local command_name="$1"
    command -v "${command_name}" >/dev/null 2>&1 || \
        fail "required command not found: ${command_name}"
}

for command_name in bash git cmake ctest awk; do
    require_command "${command_name}"
done

[[ -x scripts/phase7/verify.sh ]] || fail "scripts/phase7/verify.sh is missing or not executable"

run_phase8_suite() {
    local build_dir="$1"
    local description="$2"
    local minimum_tests=17
    local listing
    local count

    echo "== ${description} =="
    listing="$(ctest --test-dir "${build_dir}" -N -L phase8)" || \
        fail "cannot enumerate ${description}"
    count="$(printf '%s\n' "${listing}" | awk '$1 == "Total" && $2 == "Tests:" { print $3 }')"

    [[ "${count}" =~ ^[0-9]+$ ]] || fail "cannot count ${description}"
    (( count >= minimum_tests )) || \
        fail "${description}: expected at least ${minimum_tests} tests, found ${count}"

    printf 'Discovered tests: %s (minimum %s)\n' "${count}" "${minimum_tests}"
    ctest --test-dir "${build_dir}" -L phase8 --no-tests=error --output-on-failure
}

echo "== repository scope =="
git diff --check
git diff --cached --check

echo "== inherited Phase 7 verification =="
bash ./scripts/phase7/verify.sh

echo "== fresh Phase 8 debug build =="
rm -rf build/debug
cmake --preset debug
cmake --build --preset debug -j

run_phase8_suite build/debug "Phase 8 threading tests"

echo "== Phase 8 full regression =="
ctest --test-dir build/debug --output-on-failure

echo "== fresh ThreadSanitizer build =="
rm -rf build/thread-sanitizer
cmake --preset thread-sanitizer
cmake --build --preset thread-sanitizer -j

if [[ "${RUN_TSAN_TESTS:-0}" == "1" ]]; then
    run_phase8_suite build/thread-sanitizer "Phase 8 ThreadSanitizer tests"
else
    echo "ThreadSanitizer runtime tests: SKIPPED."
    echo "Set RUN_TSAN_TESTS=1 when the host supports TSan."
    echo "GitHub CI remains the required ThreadSanitizer runtime gate."
fi

echo "Phase 8 automated verification: PASS"
