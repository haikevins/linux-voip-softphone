#!/usr/bin/env bash
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
cd "$ROOT"

fail() {
    printf 'ERROR: %s\n' "$*" >&2
    exit 1
}

for command in cmake cpack tar nproc sha256sum; do
    command -v "$command" >/dev/null 2>&1 || fail "required command not found: $command"
done

BUILD_DIR="$ROOT/build/release"
INSTALL_DIR="$ROOT/build/phase11-install"
PACKAGE_DIR="$ROOT/build/phase11-package"

printf '%s\n' '== Phase 11 release configure =='
rm -rf "$BUILD_DIR" "$INSTALL_DIR" "$PACKAGE_DIR"
cmake --preset release

printf '%s\n' '== Phase 11 release build =='
cmake --build --preset release --parallel "$(nproc)"

printf '%s\n' '== Phase 11 staged install =='
cmake --install "$BUILD_DIR" --prefix "$INSTALL_DIR"

required_binaries=(
    linux-voip-softphone
    linux-voip-softphone-gui
    rtsp-lab
    voip-sip-register
    voip-sip-call
    voip-sip-listen
)

for name in "${required_binaries[@]}"; do
    path="$INSTALL_DIR/bin/$name"
    [[ -x "$path" ]] || fail "installed release binary missing or not executable: $path"
    printf 'FOUND: %s\n' "$path"
done

printf '%s\n' '== Phase 11 installed-binary smoke =='
"$INSTALL_DIR/bin/linux-voip-softphone" >/dev/null
"$INSTALL_DIR/bin/rtsp-lab" --help >/dev/null

printf '%s\n' '== Phase 11 TGZ package =='
mkdir -p "$PACKAGE_DIR"
cpack \
    --config "$BUILD_DIR/CPackConfig.cmake" \
    -G TGZ \
    -B "$PACKAGE_DIR"

mapfile -t archives < <(
    find "$PACKAGE_DIR" -maxdepth 1 -type f \
        -name 'linux-voip-softphone-1.0.0-*.tar.gz' \
        -print | sort
)

[[ "${#archives[@]}" -eq 1 ]] || \
    fail "expected exactly one Phase 11 TGZ package, found ${#archives[@]}"

archive="${archives[0]}"
package_root="$(basename "$archive" .tar.gz)"
listing="$(mktemp)"
trap 'rm -f "$listing"' EXIT

tar -tzf "$archive" >"$listing"

for name in "${required_binaries[@]}"; do
    grep -Fxq "$package_root/bin/$name" "$listing" || \
        fail "packaged binary missing: $package_root/bin/$name"
done

printf 'Release package: %s\n' "$archive"
printf 'Release package SHA256: '
sha256sum "$archive" | awk '{print $1}'
printf '%s\n' 'Phase 11 release build and packaging verification: PASS'
