#!/usr/bin/env bash
# P8-R3C: single opt-in, bounded real-vendor callback dispatch observation.
# Build is isolated: never patch or install into the user's PJPROJECT checkout.
set -euo pipefail

root="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/../.." && pwd -P)"
cd "$root"
fail() { printf 'ABORT: %s\n' "$*" >&2; exit 1; }

# Reject unapproved traffic BEFORE touching source, building, or asking for SIP secrets.
[[ "${PHASE8_LIVE_SIP:-}" == '1' ]] || fail 'PHASE8_LIVE_SIP=1 is required for SIP traffic'
[[ "${PHASE8_R3C_CONFIRM:-}" == '1002-to-1001' ]] || \
    fail 'PHASE8_R3C_CONFIRM=1002-to-1001 is required (192.168.1.23, UDP)'
for tool in git cmake make pkg-config python3 nm timeout tar sha256sum ninja; do
    command -v "$tool" >/dev/null 2>&1 || fail "missing required tool: $tool"
done

[[ "$(git branch --show-current)" == 'phase8/real-vendor-callback-dispatch' ]] || \
    fail 'unexpected branch; lab must run on P8-R3C review branch'
[[ -n "$(git rev-parse HEAD 2>/dev/null)" ]] || fail 'not a Git repository'
baseline='40cffc5679b6b6ece9cc70b2d96ca1ad2d9fe329'
if [[ "$(git rev-parse HEAD)" != "$baseline" &&
      "$(git rev-parse HEAD^)" != "$baseline" ]]; then
    fail 'P8-R3C branch is not directly based on 40cffc5'
fi
# Source checkout is deliberately dirty before commit: changes are restricted
# to audited, lab-only files and tests. No cleanup or resets are performed.

vendor_source="${PHASE8_R3C_VENDOR_SOURCE:-$HOME/.local/src/pjproject-2.17}"
[[ -d "$vendor_source/.git" || -f "$vendor_source/.git" ]] || fail 'vendor Git checkout not found'
rev='5a457451fa2712ba18e12b01738e8ff3af2b26fd'
[[ "$(git -C "$vendor_source" rev-parse HEAD)" == "$rev" ]] || fail 'wrong PJPROJECT source revision'
[[ -z "$(git -C "$vendor_source" status --porcelain --untracked-files=all)" ]] || \
    fail 'PJPROJECT source checkout is not clean'
expected_endpoint_sha='0bd796348c29272ceb98d49bd15574f02935bb28c4c268ea92fe7fdec4ed727c'
actual_endpoint_sha="$(sha256sum "$vendor_source/pjsip/src/pjsua2/endpoint.cpp" | cut -d' ' -f1)"
[[ "$actual_endpoint_sha" == "$expected_endpoint_sha" ]] || fail 'vendor endpoint.cpp SHA256 mismatch'

patch="$root/labs/pjsip/callback-dispatch/pjproject-2.17-vendor-hook.patch"
[[ -f "$patch" ]] || fail 'missing reviewed vendor-only patch'
private="$root/build/phase8-r3c-vendor-lab"
vendor="$private/vendor"
prefix="$private/install"
app_build="$private/application"
if [[ -e "$private" ]]; then
    # Never reuse a previous instrumented library or overwrite an old lab.
    fail "isolated build already exists: $private (inspect/move aside explicitly before re-running)"
fi
mkdir -p -- "$vendor" "$prefix"
umask 077
ulimit -c 0 || true
raw="$(mktemp "${TMPDIR:-/tmp}/phase8-r3c-private-XXXXXXXX")"
trap 'rm -f -- "$raw"; unset VOIP_SIP_PASSWORD' EXIT

printf 'P8-R3C: exporting verified PJPROJECT 2.17 into isolated build\n'
# No .git copying and no mutation of the operator's vendor checkout.
git -C "$vendor_source" archive "$rev" | tar -xf - -C "$vendor"
git -C "$vendor" apply --unidiff-zero --check "$patch" || fail 'vendor patch does not apply'
git -C "$vendor" apply --unidiff-zero "$patch" || fail 'vendor patch failed'

printf 'P8-R3C: building isolated instrumented static vendor libraries\n'
if ! (cd "$vendor" && \
    CXXFLAGS='-O0 -g -DVOIP_PHASE8_R3C_VENDOR_LAB=1' \
    CFLAGS='-O0 -g' \
    ./configure --prefix="$prefix") >"$private/vendor-configure.log" 2>&1; then
    fail "PJPROJECT configure failed; see $private/vendor-configure.log"
fi
if ! make -C "$vendor" -j 2 >"$private/vendor-build.log" 2>&1; then
    fail "PJPROJECT isolated build failed; see $private/vendor-build.log"
fi
if ! make -C "$vendor" install >"$private/vendor-install.log" 2>&1; then
    fail "PJPROJECT isolated install failed; see $private/vendor-install.log"
fi

shopt -s nullglob
vendor_archives=("$prefix"/lib/libpjsua2*.a)
[[ "${#vendor_archives[@]}" -eq 1 ]] || fail 'expected one isolated libpjsua2 archive'
nm "${vendor_archives[0]}" > "$private/vendor-symbols.txt" || fail 'nm failed'
if ! grep -q 'voip_phase8_r3c_vendor_after_lookup' "$private/vendor-symbols.txt"; then
    fail 'instrumentation symbol missing from isolated vendor archive'
fi
export PKG_CONFIG_PATH="$prefix/lib/pkgconfig"
# Prevent a fallback to the normal installed PJPROJECT pkg-config metadata.
export PKG_CONFIG_LIBDIR="$prefix/lib/pkgconfig"
[[ "$(pkg-config --modversion libpjproject)" == '2.17' ]] || fail 'isolated PJPROJECT pkg-config version mismatch'
[[ "$(pkg-config --variable=libdir libpjproject)" == "$prefix/lib" ]] || \
    fail 'pkg-config selected a non-isolated library directory'

printf 'P8-R3C: linking dedicated client probe against isolated static vendor build\n'
if ! cmake -S "$root" -B "$app_build" -G Ninja \
    -DCMAKE_BUILD_TYPE=Debug -DENABLE_PJSIP=ON -DENABLE_QT_GUI=OFF \
    -DENABLE_PHASE8_R3C_LAB=ON -DBUILD_TESTING=OFF >"$private/app-configure.log" 2>&1; then
    fail "probe CMake configure failed; see $private/app-configure.log"
fi
if ! cmake --build "$app_build" --target phase8_r3c_vendor_dispatch -j 2 >"$private/app-build.log" 2>&1; then
    fail "probe build/link failed; see $private/app-build.log"
fi
bin="$app_build/src/phase8_r3c_vendor_dispatch"
[[ -x "$bin" ]] || fail 'instrumented probe executable missing'

if [[ -z "${VOIP_SIP_PASSWORD:-}" ]]; then
    [[ -t 0 ]] || fail 'SIP password is unset and stdin is not interactive'
    read -rsp 'SIP password for lab extension 1002: ' VOIP_SIP_PASSWORD
    printf '\n'
    [[ -n "$VOIP_SIP_PASSWORD" ]] || fail 'empty SIP password'
    export VOIP_SIP_PASSWORD
fi
log="${PHASE8_R3C_LOG:-$HOME/Downloads/phase8-r3c-$(date +%Y%m%d-%H%M%S).txt}"
mkdir -p -- "$(dirname -- "$log")"
[[ ! -e "$log" && ! -L "$log" ]] || fail 'summary file already exists'

printf 'P8-R3C: ONE approved SIP call, Asterisk 192.168.1.23 1002->1001; target may ring.\n'
set +e
timeout --signal=TERM --kill-after=5s 90s "$bin" >"$raw" 2>&1
rc=$?
set -e

# Only allowlisted, fixed-value status markers. No raw PJSIP logs or secrets.
{
    printf 'P8R3C_BASELINE=40cffc5\n'
    printf 'P8R3C_VENDOR_REVISION=5a457451\n'
    printf 'P8R3C_VENDOR_BUILD=ISOLATED_STATIC_INSTRUMENTED\n'
    grep -E '^P8R3C_(REGISTRATION|CALL_ATTEMPT|DISCONNECTION|RELEASE_DISCONNECTED|STOP|DESTRUCTOR|REAL_VENDOR_PRE_GATE_PAUSE|VENDOR_TO_APP_INGRESS|NO_RETIRE_WHILE_PAUSED|EXTERNAL_CALLBACK_QUIESCENCE|INTEGRATION_READINESS|RESULT)=(PASS|FAIL|STARTED|RETURNED|OBSERVED|INCONCLUSIVE|YES|UNPROVEN|BLOCKED|FAIL_EXCEPTION|FAIL_UNKNOWN)$' "$raw" || true
    printf 'P8R3C_PROCESS_EXIT=%d\n' "$rc"
} >"$log"
cat "$log"
printf 'P8R3C_SUMMARY_LOG=%s\n' "$log"
python3 -B "$root/scripts/phase8/verify-r3c-summary.py" "$log" || \
    fail 'real-vendor observation FAIL/INCONCLUSIVE; no callback-quiescence proof'
printf 'PASS: bounded real-vendor pre-gate dispatch observed; quiescence UNPROVEN\n'
