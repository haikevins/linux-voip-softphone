#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
EVIDENCE_DIR="${ROOT_DIR}/evidence/phase4/call-flow"

usage() {
    cat <<'USAGE'
Usage:
  sudo ./scripts/phase4/verify-call.sh --check
  sudo ./scripts/phase4/verify-call.sh --capture

--check     Verify that endpoints 1001 and 1002 are registered and that one
            established call has both PJSIP channels in state Up.
--capture   Run the same verification and record selected non-secret Asterisk
            state under evidence/phase4/call-flow/.
USAGE
}

fail() {
    printf 'ERROR: %s\n' "$*" >&2
    exit 1
}

asterisk_cli() {
    local command="$1"
    asterisk -rx "${command}" 2>&1
}

require_asterisk_runtime() {
    command -v asterisk >/dev/null 2>&1 || \
        fail "asterisk is not installed"

    local version_output
    if ! version_output="$(asterisk_cli "core show version")"; then
        printf '%s\n' "${version_output}" >&2
        fail "cannot connect to the Asterisk CLI; ensure Asterisk is running and retry with sudo"
    fi

    local module_output
    module_output="$(asterisk_cli "module show like res_pjsip.so")"
    grep -Fq "res_pjsip.so" <<<"${module_output}" || \
        fail "res_pjsip.so is not loaded"
}

verify_registered_contacts() {
    local contacts_output="$1"

    grep -Eq "Contact:[[:space:]]+1001/" <<<"${contacts_output}" || \
        fail "global contact table is missing endpoint 1001"
    grep -Eq "Contact:[[:space:]]+1002/" <<<"${contacts_output}" || \
        fail "global contact table is missing endpoint 1002"
}

verify_call() {
    echo "== SIP call verification =="
    require_asterisk_runtime

    local contacts_output
    local channels_output
    local pjsip_channels_output

    contacts_output="$(asterisk_cli "pjsip show contacts")"
    channels_output="$(asterisk_cli "core show channels verbose")"
    pjsip_channels_output="$(asterisk_cli "pjsip show channels")"

    verify_registered_contacts "${contacts_output}"

    grep -Eq '^PJSIP/1001-[^[:space:]]+[[:space:]].*[[:space:]]Up[[:space:]]' \
        <<<"${channels_output}" || \
        fail "endpoint 1001 does not have an Up PJSIP channel"
    grep -Eq '^PJSIP/1002-[^[:space:]]+[[:space:]].*[[:space:]]Up[[:space:]]' \
        <<<"${channels_output}" || \
        fail "endpoint 1002 does not have an Up PJSIP channel"

    grep -Eq 'Channel:[[:space:]]+PJSIP/1001-.*[[:space:]]Up[[:space:]]' \
        <<<"${pjsip_channels_output}" || \
        fail "pjsip show channels does not report endpoint 1001 as Up"
    grep -Eq 'Channel:[[:space:]]+PJSIP/1002-.*[[:space:]]Up[[:space:]]' \
        <<<"${pjsip_channels_output}" || \
        fail "pjsip show channels does not report endpoint 1002 as Up"

    grep -Eq '^2 active channels$' <<<"${channels_output}" || \
        fail "expected exactly 2 active channels"
    grep -Eq '^1 active call$' <<<"${channels_output}" || \
        fail "expected exactly 1 active call"

    echo "Endpoint 1001 active channel: PASS"
    echo "Endpoint 1002 active channel: PASS"
    echo "Active call count: PASS"
    echo
    printf '%s\n' "${channels_output}"
    echo
    echo "SIP call verification: PASS"
}

clean_evidence_files() {
    local evidence_file
    local tmp_file

    while IFS= read -r -d "" evidence_file; do
        tmp_file="${evidence_file}.tmp"
        sed 's/[[:space:]]\+$//' "${evidence_file}" |
            tac |
            sed '/./,$!d' |
            tac >"${tmp_file}"
        mv "${tmp_file}" "${evidence_file}"
    done < <(find "${EVIDENCE_DIR}" -maxdepth 1 -type f -name '*.txt' -print0)
}

capture_evidence() {
    mkdir -p "${EVIDENCE_DIR}"

    local captured_at
    local branch
    local commit
    captured_at="$(date --iso-8601=seconds)"
    branch="$(git -C "${ROOT_DIR}" branch --show-current 2>/dev/null || true)"
    commit="$(git -C "${ROOT_DIR}" rev-parse HEAD 2>/dev/null || true)"

    asterisk_cli "core show channels verbose" >"${EVIDENCE_DIR}/channels-verbose.txt"
    asterisk_cli "pjsip show channels" >"${EVIDENCE_DIR}/pjsip-channels.txt"
    asterisk_cli "pjsip show contacts" >"${EVIDENCE_DIR}/contacts.txt"

    {
        echo "Phase 4 SIP call-flow evidence"
        echo "Captured at: ${captured_at}"
        echo "Branch: ${branch:-unknown}"
        echo "Commit: ${commit:-unknown}"
        echo "Caller endpoint: 1001"
        echo "Callee endpoint: 1002"
        echo "Expected active channels: 2"
        echo "Expected active calls: 1"
        echo "Verification command: sudo ./scripts/phase4/verify-call.sh --check"
    } >"${EVIDENCE_DIR}/call-summary.txt"

    clean_evidence_files

    if [[ -n "${SUDO_UID:-}" && -n "${SUDO_GID:-}" ]]; then
        chown -R "${SUDO_UID}:${SUDO_GID}" "${EVIDENCE_DIR}"
    fi

    echo "Call-flow evidence written to: ${EVIDENCE_DIR}"
}

mode="${1:---check}"
case "${mode}" in
    --check)
        verify_call
        ;;
    --capture)
        verify_call
        capture_evidence
        ;;
    --help|-h)
        usage
        ;;
    *)
        usage >&2
        exit 2
        ;;
esac
