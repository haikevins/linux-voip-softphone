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
Usage: ./scripts/phase9/capture-core-dump.sh [output-directory]

Builds the Phase 9 crash generator with debug symbols, stops it on the
intentional SIGSEGV under GDB, writes a reproducible core file, and analyzes
that saved core in a second GDB invocation.

Default output directory:
  build/phase9-core

Generated files:
  crash_generator.core
  gdb-live.txt
  gdb-core-backtrace.txt
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

for command_name in cmake gdb grep mkdir rm test; do
    require_command "${command_name}"
done

BUILD_DIR="${ROOT_DIR}/build/debug"
BINARY="${BUILD_DIR}/tools/crash_generator/crash_generator"
OUTPUT_DIR="${1:-${ROOT_DIR}/build/phase9-core}"
CORE_FILE="${OUTPUT_DIR}/crash_generator.core"
LIVE_LOG="${OUTPUT_DIR}/gdb-live.txt"
CORE_LOG="${OUTPUT_DIR}/gdb-core-backtrace.txt"

echo "== Phase 9 crash-generator debug build =="
cmake --preset debug
cmake --build --preset debug -j --target crash_generator

[[ -x "${BINARY}" ]] || fail "crash generator was not built: ${BINARY}"

mkdir -p "${OUTPUT_DIR}"
rm -f "${CORE_FILE}" "${LIVE_LOG}" "${CORE_LOG}"

# Keep the conventional Linux core-dump limit enabled. The core used by this
# workflow is emitted explicitly by GDB so the result is deterministic even on
# hosts whose /proc/sys/kernel/core_pattern redirects crashes to another
# service such as systemd-coredump or Apport.
ulimit -c unlimited || fail "cannot enable core dumps with ulimit"

echo "== stop on the intentional SIGSEGV and save a core =="
gdb -q -batch \
    -ex "set pagination off" \
    -ex "set confirm off" \
    -ex "handle SIGSEGV stop print nopass" \
    -ex "run" \
    -ex "generate-core-file ${CORE_FILE}" \
    -ex "bt" \
    --args "${BINARY}" null-deref \
    >"${LIVE_LOG}" 2>&1

[[ -s "${CORE_FILE}" ]] || fail "GDB did not create a non-empty core file"

echo "== analyze the saved core in a fresh GDB session =="
gdb -q -batch \
    -ex "set pagination off" \
    -ex "bt" \
    "${BINARY}" "${CORE_FILE}" \
    >"${CORE_LOG}" 2>&1

grep -q "SIGSEGV" "${CORE_LOG}" || \
    fail "saved-core analysis does not report SIGSEGV"
grep -q "triggerNullDereference" "${CORE_LOG}" || \
    fail "saved-core backtrace does not contain triggerNullDereference"
grep -q "main" "${CORE_LOG}" || \
    fail "saved-core backtrace does not contain main"

printf 'Core dump: %s\n' "${CORE_FILE}"
printf 'Live GDB transcript: %s\n' "${LIVE_LOG}"
printf 'Saved-core backtrace: %s\n' "${CORE_LOG}"
echo "Phase 9 core-dump/GDB verification: PASS"
