#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
FINAL_EVIDENCE_DIR="${ROOT_DIR}/evidence/phase4/final"
CALL_EVIDENCE_DIR="${ROOT_DIR}/evidence/phase4/call-flow"
PROTOCOL_EVIDENCE_DIR="${ROOT_DIR}/evidence/phase4/protocol"

usage() {
    cat <<'USAGE'
Usage:
  sudo ./scripts/verify_phase4.sh --check
  sudo ./scripts/verify_phase4.sh --capture

--check     Verify the final Phase 4 runtime state and previously captured SIP evidence.
--capture   Run the same verification and record selected non-secret final evidence under
            evidence/phase4/final/.
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

normalize_text_files() {
    local directory="$1"
    local evidence_file
    local tmp_file

    while IFS= read -r -d '' evidence_file; do
        tmp_file="${evidence_file}.tmp"
        sed 's/[[:space:]]\+$//' "${evidence_file}" |
            tac |
            sed '/./,$!d' |
            tac >"${tmp_file}"
        mv "${tmp_file}" "${evidence_file}"
    done < <(find "${directory}" -maxdepth 1 -type f -name '*.txt' -print0)
}

verify_live_runtime() {
    "${ROOT_DIR}/scripts/verify_asterisk.sh" --runtime >/dev/null
    echo "Asterisk runtime: PASS"

    "${ROOT_DIR}/scripts/verify_sip_registration.sh" --check >/dev/null
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

verify_protocol_evidence() {
    local summary_file="${PROTOCOL_EVIDENCE_DIR}/call-flow-summary.txt"
    require_file "${summary_file}"

    require_match '401 Unauthorized' "${summary_file}" \
        "protocol evidence is missing the Digest challenge"
    require_match '180 Ringing' "${summary_file}" \
        "protocol evidence is missing ringing"
    require_match '200 OK' "${summary_file}" \
        "protocol evidence is missing successful SIP responses"
    require_match 'ACK' "${summary_file}" \
        "protocol evidence is missing ACK"
    require_match 'BYE' "${summary_file}" \
        "protocol evidence is missing BYE teardown"
    require_match 'PCMU/8000' "${summary_file}" \
        "protocol evidence is missing negotiated PCMU/8000 media"
    require_match 'B2BUA' "${summary_file}" \
        "protocol evidence is missing B2BUA behavior"

    echo "Sanitized protocol evidence: PASS"
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
    verify_protocol_evidence
    verify_sensitive_material_absent
    echo
    echo "Phase 4 final verification: PASS"
}

capture_evidence() {
    mkdir -p "${FINAL_EVIDENCE_DIR}"

    local captured_at
    local branch
    local commit
    captured_at="$(date --iso-8601=seconds)"
    branch="$(git -C "${ROOT_DIR}" branch --show-current 2>/dev/null || true)"
    commit="$(git -C "${ROOT_DIR}" rev-parse HEAD 2>/dev/null || true)"

    asterisk_cli 'pjsip show contacts' >"${FINAL_EVIDENCE_DIR}/contacts.txt"
    asterisk_cli 'core show channels' >"${FINAL_EVIDENCE_DIR}/channels.txt"
    git -C "${ROOT_DIR}" log --oneline --decorate -10 \
        >"${FINAL_EVIDENCE_DIR}/git-log.txt" 2>/dev/null || true

    {
        echo "Phase 4 final verification evidence"
        echo "Captured at: ${captured_at}"
        echo "Branch: ${branch:-unknown}"
        echo "Commit: ${commit:-unknown}"
        echo "Asterisk runtime: PASS"
        echo "SIP registration: PASS"
        echo "SIP contact reachability: PASS"
        echo "Idle channel state: PASS"
        echo "Established-call evidence: PASS"
        echo "Sanitized protocol evidence: PASS"
        echo "Sensitive SIP material check: PASS"
        echo "Final result: PASS"
        echo "Verification command: sudo ./scripts/verify_phase4.sh --check"
    } >"${FINAL_EVIDENCE_DIR}/phase4-summary.txt"

    normalize_text_files "${FINAL_EVIDENCE_DIR}"

    if [[ -n "${SUDO_UID:-}" && -n "${SUDO_GID:-}" ]]; then
        chown -R "${SUDO_UID}:${SUDO_GID}" "${FINAL_EVIDENCE_DIR}"
    fi

    echo "Phase 4 final evidence written to: ${FINAL_EVIDENCE_DIR}"
}

mode="${1:---check}"
case "${mode}" in
    --check)
        verify_phase4
        ;;
    --capture)
        verify_phase4
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
