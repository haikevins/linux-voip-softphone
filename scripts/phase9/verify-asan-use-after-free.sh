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
Usage: ./scripts/phase9/verify-asan-use-after-free.sh [output-directory]

Builds the Phase 9 crash generator with the repository ASan+UBSan preset,
runs the repaired lifetime path for the historical use-after-free scenario,
and verifies that the same path now completes without sanitizer diagnostics.

Default output directory:
  build/phase9-asan

Generated file:
  asan-use-after-free.txt
EOF
}

if [[ "${1:-}" == "--help" || "${1:-}" == "-h" ]]; then
    usage
    exit 0
fi

if (( $# > 1 )); then
    usage >&2
    exit 2
fi

for command_name in cmake grep mkdir rm; do
    require_command "${command_name}"
done

BUILD_DIR="${ROOT_DIR}/build/sanitizers"
BINARY="${BUILD_DIR}/tools/crash_generator/crash_generator"
OUTPUT_DIR="${1:-${ROOT_DIR}/build/phase9-asan}"
LOG_FILE="${OUTPUT_DIR}/asan-use-after-free.txt"

echo "== fresh Phase 9 ASan crash-generator build =="
# Packages are applied with checksum rsync while preserving mtimes. Remove the
# disposable sanitizer build tree so Ninja cannot reuse stale objects.
rm -rf "${BUILD_DIR}"

cmake --preset sanitizers
cmake --build --preset sanitizers -j --target crash_generator

[[ -x "${BINARY}" ]] || fail "crash generator was not built: ${BINARY}"

mkdir -p "${OUTPUT_DIR}"
rm -f "${LOG_FILE}"

echo "== repaired lifetime path under AddressSanitizer =="
set +e
ASAN_OPTIONS="halt_on_error=1:abort_on_error=1:detect_leaks=0:symbolize=1" \
UBSAN_OPTIONS="halt_on_error=1:print_stacktrace=1" \
    "${BINARY}" use-after-free >"${LOG_FILE}" 2>&1
status=$?
set -e

if (( status != 0 )); then
    cat "${LOG_FILE}" >&2
    fail "repaired lifetime path exited with status ${status}"
fi

if grep -Eq "ERROR: AddressSanitizer|runtime error:|heap-use-after-free" "${LOG_FILE}"; then
    cat "${LOG_FILE}" >&2
    fail "sanitizer diagnostics remain after the lifetime fix"
fi

grep -q "Repaired lifetime path completed without accessing freed memory." \
    "${LOG_FILE}" || \
    fail "success marker is missing from the repaired lifetime path"

printf 'ASan log: %s\n' "${LOG_FILE}"
echo "Phase 9 ASan lifetime-fix verification: PASS"
