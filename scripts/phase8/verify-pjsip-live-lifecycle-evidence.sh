#!/usr/bin/env bash
# Offline validation of the sanitized live lifecycle evidence recorded for Phase 8.
set -euo pipefail

ROOT="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/../.." && pwd -P)"
cd "$ROOT"

EVIDENCE='evidence/phase8/pjsip-live-lifecycle.txt'
DOC='docs/phase8/commit30-pjsip-live-lifecycle-evidence.md'
RUNNER='scripts/phase8/run-pjsip-live-lifecycle-probe.sh'
SOURCE='tests/phase8/pjsip_live_lifecycle_probe.cpp'

for path in "$EVIDENCE" "$DOC" "$RUNNER" "$SOURCE"; do
    [[ -s "$path" ]] || {
        printf 'ERROR: missing or empty required artifact: %s\n' "$path" >&2
        exit 1
    }
done

required_markers=(
    'BASELINE_HEAD=4ace2be'
    'PJSIP_VERSION=2.17'
    'REGISTERED=yes status=200'
    'ACTIVE_CALL_SLOTS=1'
    'FIRST_SHUTDOWN=PJ_EBUSY'
    'CALL_SLOTS_AFTER_HANGUP=0'
    'SECOND_SHUTDOWN=SUCCESS'
    'ACCOUNT_CALLBACKS_ACTIVE_AT_RETURN=0'
    'ACCOUNT_CALLBACKS_DURING_GRACE=0'
    'POST_SHUTDOWN_CALLBACKS=0'
    'LIB_DESTROY=SUCCESS'
    'POST_LIBDESTROY_CALLBACKS=0'
    'PROBE_RESULT=PASS'
    'ORIGINAL_TEMP_LOG_SHA256=77c62f614fef037c81aa254ebab2352a557197d2c278a6927a90084d93d481f5'
    'CONTROLLED_PJSUA2_PJ_EBUSY_RECOVERY=LIVE_OBSERVED_EVIDENCED'
    'LIVE_EXTERNAL_CALLBACK_QUIESCENCE=UNPROVEN'
    'LATE_REGISTRATION_CALLBACKS_IN_RECORDED_RUN=NONE_OBSERVED'
    'PRODUCTION_INTEGRATION_READINESS=BLOCKED'
)

for marker in "${required_markers[@]}"; do
    grep -Fxq "$marker" "$EVIDENCE" || {
        printf 'ERROR: missing live evidence marker: %s\n' "$marker" >&2
        exit 1
    }
done

if grep -Ein \
    '^[[:space:]]*(Proxy-)?Authorization:[[:space:]]*Digest|nonce="|cnonce="|response="|a=crypto:.*inline:' \
    "$EVIDENCE" >/dev/null 2>&1; then
    echo 'ERROR: secret-bearing SIP/SRTP material detected in live evidence' >&2
    exit 1
fi

grep -Fq 'Live external callback quiescence: **UNPROVEN**' "$DOC"
grep -Fq 'Production integration readiness: **BLOCKED**' "$DOC"
grep -Fq 'controlled PJSUA2 `PJ_EBUSY` recovery sequence' "$DOC"

grep -Fq 'PHASE8_LIVE_SIP' "$RUNNER"
grep -Fq 'pkg-config --libs --static libpjproject' "$RUNNER"
grep -Fq 'shutdown.force = false;' "$SOURCE"
grep -Fq 'FIRST_SHUTDOWN=PJ_EBUSY' "$SOURCE"
grep -Fq 'pjsua_call_get_count() == 0U' "$SOURCE"

printf '%s\n' 'PASS: recorded controlled PJSUA2 live lifecycle evidence'
printf '%s\n' 'Controlled PJSUA2 PJ_EBUSY recovery: LIVE OBSERVED / EVIDENCED'
printf '%s\n' 'Late registration callbacks after shutdown2: NONE OBSERVED IN RECORDED LIVE RUN'
printf '%s\n' 'Live external callback quiescence: UNPROVEN'
printf '%s\n' 'Production integration readiness: BLOCKED'
