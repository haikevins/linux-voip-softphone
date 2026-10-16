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

usage() {
    cat <<'EOF'
Usage: ./scripts/phase9/verify.sh

Runs inherited Phase 8 verification, Phase 9 safe CTest regression,
GDB saved-core analysis, and the repaired-lifetime ASan+UBSan check.

Requires GDB. Core dumps and sanitizer logs remain under build/ and
must never be committed as evidence.
EOF
}

if [[ "${1:-}" == "--help" || "${1:-}" == "-h" ]]; then
    usage
    exit 0
fi
if (( $# != 0 )); then
    usage >&2
    exit 2
fi

for command_name in bash git cmake ctest awk gdb; do
    require_command "${command_name}"
done

for script in \
    scripts/phase8/verify.sh \
    scripts/phase9/capture-core-dump.sh \
    scripts/phase9/verify-asan-use-after-free.sh; do
    [[ -x "${script}" ]] || fail "missing executable verifier: ${script}"
done

verify_phase9_ctest() {
    local build_dir="$1"
    local listing
    local count

    listing="$(ctest --test-dir "${build_dir}" -N -L phase9)" || \
        fail "cannot enumerate Phase 9 tests in ${build_dir}"
    count="$(printf '%s\n' "${listing}" | awk '$1 == "Total" && $2 == "Tests:" { print $3 }')"
    [[ "${count}" =~ ^[0-9]+$ ]] || fail "cannot count Phase 9 tests in ${build_dir}"
    (( count >= 2 )) || fail "expected at least 2 Phase 9 tests, found ${count} in ${build_dir}"

    printf 'Phase 9 discovered tests in %s: %s\n' "${build_dir}" "${count}"
    ctest --test-dir "${build_dir}" -L phase9 --no-tests=error --output-on-failure
}

echo "== repository scope =="
git diff --check
git diff --cached --check

echo "== inherited Phase 8 verification =="
./scripts/phase8/verify.sh

echo "== Phase 9 normal build regression =="
verify_phase9_ctest build/debug

echo "== Phase 9 saved-core GDB analysis =="
./scripts/phase9/capture-core-dump.sh

echo "== Phase 9 ASan/UBSan repaired lifetime =="
./scripts/phase9/verify-asan-use-after-free.sh

echo "== Phase 9 sanitizer CTest regression =="
verify_phase9_ctest build/sanitizers

echo "Phase 9 automated verification: PASS"
