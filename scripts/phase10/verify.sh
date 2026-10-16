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
    cat <<'USAGE'
Usage: ./scripts/phase10/verify.sh

Runs inherited Phase 9 verification and the currently available Phase 10 RTSP
mini-lab tests. Commit 1 provides only build/test scaffolding; later commits add
protocol parsing, session workflow, RTP integration, and broader verification.
USAGE
}

if [[ "${1:-}" == "--help" || "${1:-}" == "-h" ]]; then
    usage
    exit 0
fi
if (( $# != 0 )); then
    usage >&2
    exit 2
fi

for command_name in bash git ctest awk; do
    require_command "${command_name}"
done

[[ -x scripts/phase9/verify.sh ]] || fail "scripts/phase9/verify.sh is missing or not executable"

verify_phase10_ctest() {
    local build_dir="$1"
    local listing
    local count

    listing="$(ctest --test-dir "${build_dir}" -N -L phase10)" || \
        fail "cannot enumerate Phase 10 tests in ${build_dir}"
    count="$(printf '%s\n' "${listing}" | awk '$1 == "Total" && $2 == "Tests:" { print $3 }')"

    [[ "${count}" =~ ^[0-9]+$ ]] || fail "cannot count Phase 10 tests in ${build_dir}"
    (( count >= 1 )) || fail "expected at least 1 Phase 10 test, found ${count} in ${build_dir}"

    printf 'Phase 10 discovered tests in %s: %s\n' "${build_dir}" "${count}"
    ctest --test-dir "${build_dir}" -L phase10 --no-tests=error --output-on-failure
}

echo "== repository scope =="
git diff --check
git diff --cached --check

echo "== inherited Phase 9 verification =="
./scripts/phase9/verify.sh

echo "== Phase 10 RTSP scaffold verification =="
verify_phase10_ctest build/debug

echo "Phase 10 automated verification: PASS"
