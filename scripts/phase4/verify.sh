#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
CALL_EVIDENCE_DIR="${ROOT_DIR}/evidence/phase4/call-flow"

usage() {
    cat <<'USAGE'
Usage:
  sudo ./scripts/phase4/verify.sh --check

--check   Verify the final Phase 4 runtime state, previously captured call-flow
          evidence, and evidence-safety rules.

Evidence capture is intentionally scenario-specific:
  sudo ./scripts/phase4/verify-registration.sh --capture
  sudo ./scripts/phase4/verify-call.sh --capture
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

require_file() {
    local path="$1"
    [[ -f "${path}" ]] || fail "required evidence file is missing: ${path#${ROOT_DIR}/}"
}

require_match() {
    local pattern="$1"
    local path="$2"
    local description="$3"
    grep -Eq "${pattern}" "${path}" || fail "${description}"
}

verify_live_runtime() {
    "${ROOT_DIR}/scripts/phase4/verify-asterisk.sh" --runtime >/dev/null
    echo "Asterisk runtime: PASS"

    "${ROOT_DIR}/scripts/phase4/verify-registration.sh" --check >/dev/null
    echo "SIP registration: PASS"

    local contacts_output
    contacts_output="$(asterisk_cli 'pjsip show contacts')"

    grep -Eq 'Contact:[[:space:]]+1001/.*[[:space:]]Avail([[:space:]]|$)' <<<"${contacts_output}" || \
        fail "endpoint 1001 contact is not currently Avail"
    grep -Eq 'Contact:[[:space:]]+1002/.*[[:space:]]Avail([[:space:]]|$)' <<<"${contacts_output}" || \
        fail "endpoint 1002 contact is not currently Avail"
    echo "SIP contact reachability: PASS"

    local channels_output
    channels_output="$(asterisk_cli 'core show channels')"
    grep -Fq '0 active channels' <<<"${channels_output}" || \
        fail "Asterisk is not idle: active channels remain"
    grep -Fq '0 active calls' <<<"${channels_output}" || \
        fail "Asterisk is not idle: active calls remain"
    echo "Idle channel state: PASS"
}

verify_call_evidence() {
    local channels_file="${CALL_EVIDENCE_DIR}/channels-verbose.txt"
    local pjsip_file="${CALL_EVIDENCE_DIR}/pjsip-channels.txt"

    require_file "${channels_file}"
    require_file "${pjsip_file}"

    require_match '^2 active channels$' "${channels_file}" \
        "call-flow evidence does not record two active channels"
    require_match '^1 active call$' "${channels_file}" \
        "call-flow evidence does not record one active call"
    require_match '^PJSIP/1001-.*[[:space:]]Up[[:space:]]' "${channels_file}" \
        "call-flow evidence does not record endpoint 1001 in state Up"
    require_match '^PJSIP/1002-.*[[:space:]]Up[[:space:]]' "${channels_file}" \
        "call-flow evidence does not record endpoint 1002 in state Up"
    require_match 'Channel: PJSIP/1001-.*[[:space:]]Up[[:space:]]' "${pjsip_file}" \
        "PJSIP channel evidence does not record endpoint 1001 in state Up"
    require_match 'Channel: PJSIP/1002-.*[[:space:]]Up[[:space:]]' "${pjsip_file}" \
        "PJSIP channel evidence does not record endpoint 1002 in state Up"

    echo "Established-call evidence: PASS"
}

verify_sensitive_material_absent() {
    local matches
    matches="$(grep -RniE \
        'Authorization:[[:space:]]*Digest|Proxy-Authorization:|a=crypto:|inline:' \
        "${ROOT_DIR}/docs/phase4" \
        "${ROOT_DIR}/evidence/phase4" \
        --exclude='README.md' 2>/dev/null || true)"

    if [[ -n "${matches}" ]]; then
        printf '%s\n' "${matches}" >&2
        fail "raw SIP authorization or media keying material found in Phase 4 artifacts"
    fi

    echo "Sensitive SIP material check: PASS"
}

verify_phase4() {
    echo "== Phase 4 final verification =="
    verify_live_runtime
    verify_call_evidence
    verify_sensitive_material_absent
    echo
    echo "Phase 4 final verification: PASS"
}

mode="${1:---check}"
case "${mode}" in
    --check)
        verify_phase4
        ;;
    --help|-h)
        usage
        ;;
    *)
        usage >&2
        exit 2
        ;;
esac
