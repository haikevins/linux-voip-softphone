#!/usr/bin/env bash
# Explicit opt-in live PJSUA2 lifecycle probe for the controlled Phase 8 lab.
# This script performs real SIP registration and a short outgoing call attempt.
set -euo pipefail

ROOT="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/../.." && pwd -P)"
cd "$ROOT"

fail() {
    printf 'ERROR: %s\n' "$*" >&2
    exit 1
}

[[ "${PHASE8_LIVE_SIP:-}" == '1' ]] || \
    fail 'set PHASE8_LIVE_SIP=1 to acknowledge that this performs real SIP traffic'

git rev-parse --is-inside-work-tree >/dev/null 2>&1 || fail 'run inside the project Git checkout'

initial_status="$(git status --porcelain=v1 --untracked-files=all)"
if [[ -n "$initial_status" && "${PHASE8_ALLOW_DIRTY:-}" != '1' ]]; then
    fail 'source checkout must be clean (or set PHASE8_ALLOW_DIRTY=1 for guarded pre-commit validation)'
fi
if [[ -n "$initial_status" ]]; then
    printf '%s\n' 'WARNING: running live probe against an intentionally dirty checkout.' >&2
fi

export PYTHONDONTWRITEBYTECODE=1
export PKG_CONFIG_PATH="${PHASE8_PJSIP_PKGCONFIG:-$HOME/.local/pjproject-2.17/lib/pkgconfig}${PKG_CONFIG_PATH:+:$PKG_CONFIG_PATH}"

command -v pkg-config >/dev/null 2>&1 || fail 'pkg-config not found'
command -v g++ >/dev/null 2>&1 || fail 'g++ not found'
[[ "$(pkg-config --modversion libpjproject)" == '2.17' ]] || \
    fail 'this recorded probe contract requires libpjproject 2.17'

export PHASE8_SIP_USER="${PHASE8_SIP_USER:-1002}"
export PHASE8_SIP_SERVER="${PHASE8_SIP_SERVER:-192.168.1.23}"
export PHASE8_SIP_TARGET="${PHASE8_SIP_TARGET:-1001}"

if [[ -z "${VOIP_SIP_PASSWORD:-}" ]]; then
    [[ -t 0 ]] || fail 'VOIP_SIP_PASSWORD is unset and stdin is not a terminal'
    read -rsp "SIP password for extension ${PHASE8_SIP_USER}: " VOIP_SIP_PASSWORD
    printf '\n'
    [[ -n "$VOIP_SIP_PASSWORD" ]] || fail 'empty SIP password'
    export VOIP_SIP_PASSWORD
fi

src='labs/pjsip/live-lifecycle/pjsip_live_lifecycle_probe.cpp'
[[ -s "$src" ]] || fail "missing live probe source: $src"

bin="$(mktemp /tmp/phase8-pjsip-live-probe.XXXXXX)"
log="${PHASE8_LIVE_LOG:-$(mktemp /tmp/phase8-pjsip-live-probe.XXXXXX.log)}"
trap 'rm -f "$bin"; unset VOIP_SIP_PASSWORD' EXIT

read -r -a pj_cflags <<< "$(pkg-config --cflags libpjproject)"
read -r -a pj_libs <<< "$(pkg-config --libs --static libpjproject)"

g++ \
    -std=c++20 \
    -Wall \
    -Wextra \
    -Wpedantic \
    -pthread \
    "${pj_cflags[@]}" \
    "$src" \
    -o "$bin" \
    "${pj_libs[@]}"

printf '%s\n' '===== LIVE PJSUA2 LIFECYCLE EXPERIMENT ====='
printf 'account=%s\nserver=%s\ntarget=%s\n' \
    "$PHASE8_SIP_USER" "$PHASE8_SIP_SERVER" "$PHASE8_SIP_TARGET"
printf '%s\n' 'NOTICE: the target may ring briefly; do not answer.'

set +e
"$bin" 2>&1 \
    | sed -E \
        -e 's/(Proxy-)?Authorization:.*/\1Authorization: [REDACTED]/I' \
        -e 's/(nonce|cnonce|response)="[^"]*"/\1="[REDACTED]"/Ig' \
    | tee "$log"
probe_rc=${PIPESTATUS[0]}
set -e

printf '\n%s\n' '===== LIVE LIFECYCLE SUMMARY ====='
grep -E \
    '^(REGISTERED|ACTIVE_CALL_SLOTS|FIRST_SHUTDOWN|CALL_SLOTS_AFTER_HANGUP|SECOND_SHUTDOWN|ACCOUNT_CALLBACKS_ACTIVE_AT_RETURN|ACCOUNT_CALLBACKS_DURING_GRACE|POST_SHUTDOWN_CALLBACKS|LIB_DESTROY|POST_LIBDESTROY_CALLBACKS|PROBE_RESULT)=' \
    "$log" || true
printf 'PROBE_EXIT=%d\n' "$probe_rc"
printf 'SAFE_LOG=%s\n' "$log"

[[ "$probe_rc" -eq 0 ]] || exit "$probe_rc"

grep -Fxq 'REGISTERED=yes status=200' "$log"
grep -Fxq 'ACTIVE_CALL_SLOTS=1' "$log"
grep -Fxq 'FIRST_SHUTDOWN=PJ_EBUSY' "$log"
grep -Fxq 'CALL_SLOTS_AFTER_HANGUP=0' "$log"
grep -Fxq 'SECOND_SHUTDOWN=SUCCESS' "$log"
grep -Fxq 'PROBE_RESULT=PASS' "$log"

if grep -Ein \
    '^[[:space:]]*(Proxy-)?Authorization:[[:space:]]*Digest|a=crypto:.*inline:' \
    "$log" >/dev/null 2>&1; then
    fail 'sanitized live log still contains a known secret-bearing marker'
fi

python3 -B - "$log" <<'PYSCAN'
import re
import sys
from pathlib import Path

text = Path(sys.argv[1]).read_text(encoding="utf-8", errors="replace")
for name, value in re.findall(r'(?i)\b(nonce|cnonce|response)="([^"]*)"', text):
    if value != '[REDACTED]':
        raise SystemExit(f'ERROR: unredacted {name} value remains in live log')
PYSCAN

final_status="$(git status --porcelain=v1 --untracked-files=all)"
[[ "$final_status" == "$initial_status" ]] || \
    fail 'live probe changed the source checkout'

printf '%s\n' 'PASS: controlled PJSUA2 live lifecycle probe'
printf '%s\n' 'BOUNDARY: this observation does not prove universal external callback quiescence.'
