#!/usr/bin/env bash
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
cd "$ROOT"

fail() {
    printf 'ERROR: %s\n' "$*" >&2
    exit 1
}

for command in tar ldd mktemp grep find timeout; do
    command -v "$command" >/dev/null 2>&1 || fail "required command not found: $command"
done

[[ -x scripts/phase11/verify-package.sh ]] || \
    fail 'scripts/phase11/verify-package.sh is missing or not executable'

printf '%s\n' '== Phase 11 fresh release artifact =='
bash scripts/phase11/verify-package.sh

PACKAGE_DIR="$ROOT/build/phase11-package"
mapfile -t archives < <(
    find "$PACKAGE_DIR" -maxdepth 1 -type f \
        -name 'linux-voip-softphone-1.0.0-*.tar.gz' \
        -print | sort
)

[[ "${#archives[@]}" -eq 1 ]] || \
    fail "expected exactly one Phase 11 TGZ package, found ${#archives[@]}"

archive="${archives[0]}"
package_root="$(basename "$archive" .tar.gz)"
work_dir="$(mktemp -d)"
listing="$work_dir/package.list"
trap 'rm -rf "$work_dir"' EXIT

tar -tzf "$archive" >"$listing"

while IFS= read -r entry; do
    [[ -n "$entry" ]] || continue
    case "$entry" in
        /*|../*|*/../*|*/..)
            fail "unsafe path in release archive: $entry"
            ;;
    esac
done <"$listing"

printf '%s\n' '== Phase 11 packaged artifact extraction =='
tar -xzf "$archive" -C "$work_dir"

bin_dir="$work_dir/$package_root/bin"
[[ -d "$bin_dir" ]] || fail "packaged bin directory missing: $bin_dir"

required_binaries=(
    linux-voip-softphone
    linux-voip-softphone-gui
    rtsp-lab
    voip-sip-register
    voip-sip-call
    voip-sip-listen
)

printf '%s\n' '== Phase 11 packaged binary dependency audit =='
for name in "${required_binaries[@]}"; do
    binary="$bin_dir/$name"
    [[ -x "$binary" ]] || fail "packaged binary missing or not executable: $name"

    dependency_log="$work_dir/$name.ldd"
    ldd "$binary" >"$dependency_log" 2>&1 || \
        fail "ldd failed for packaged binary: $name"

    if grep -Fq 'not found' "$dependency_log"; then
        cat "$dependency_log" >&2
        fail "unresolved shared-library dependency for packaged binary: $name"
    fi

    printf 'PASS dependency audit: %s\n' "$name"
done

printf '%s\n' '== Phase 11 packaged CLI smoke =='
"$bin_dir/linux-voip-softphone" >"$work_dir/app.log" 2>&1
[[ -s "$work_dir/app.log" ]] || fail 'linux-voip-softphone produced no smoke output'

for name in rtsp-lab voip-sip-register voip-sip-call voip-sip-listen; do
    "$bin_dir/$name" --help >"$work_dir/$name.help" 2>&1 || \
        fail "packaged --help smoke failed: $name"
    [[ -s "$work_dir/$name.help" ]] || fail "packaged --help output is empty: $name"
    printf 'PASS CLI smoke: %s\n' "$name"
done

printf '%s\n' '== Phase 11 packaged GUI headless startup smoke =='
set +e
QT_QPA_PLATFORM=offscreen \
    timeout 3s "$bin_dir/linux-voip-softphone-gui" \
    >"$work_dir/gui.log" 2>&1
gui_status=$?
set -e

case "$gui_status" in
    0|124)
        ;;
    *)
        cat "$work_dir/gui.log" >&2
        fail "packaged GUI failed during headless startup smoke (exit $gui_status)"
        ;;
esac

printf 'PASS GUI smoke: exit=%s (0=normal exit, 124=healthy timeout)\n' "$gui_status"
printf 'Accepted package: %s\n' "$archive"
printf '%s\n' 'BOUNDARY: this gate is offline; it does not claim a live external SIP call.'
printf '%s\n' 'Phase 11 final system acceptance verification: PASS'
