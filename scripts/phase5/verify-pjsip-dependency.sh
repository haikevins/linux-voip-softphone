#!/usr/bin/env bash
set -euo pipefail

MIN_PJSIP_VERSION="2.17"
PKG_NAME="libpjproject"

fail() {
    printf 'ERROR: %s\n' "$*" >&2
    return 1
}

command -v pkg-config >/dev/null 2>&1 || \
    fail "pkg-config is required"

command -v "${CXX:-c++}" >/dev/null 2>&1 || \
    fail "C++ compiler '${CXX:-c++}' is required"

if ! pkg-config --exists "${PKG_NAME} >= ${MIN_PJSIP_VERSION}"; then
    printf 'PJSIP dependency check: FAIL\n' >&2
    printf 'Required pkg-config module: %s >= %s\n' "${PKG_NAME}" "${MIN_PJSIP_VERSION}" >&2
    printf 'PKG_CONFIG_PATH: %s\n' "${PKG_CONFIG_PATH:-<unset>}" >&2
    return 1 2>/dev/null || exit 1
fi

version="$(pkg-config --modversion "${PKG_NAME}")"
prefix="$(pkg-config --variable=prefix "${PKG_NAME}")"

printf 'pkg-config module: %s\n' "${PKG_NAME}"
printf 'PJSIP version: %s\n' "${version}"
printf 'PJSIP prefix: %s\n' "${prefix}"

work_dir="$(mktemp -d)"
trap 'rm -rf "${work_dir}"' EXIT

cat >"${work_dir}/pjsua2-smoke.cpp" <<'CPP'
#include <pjsua2.hpp>

int main() {
    pj::EpConfig config;
    config.uaConfig.maxCalls = 4;
    return config.uaConfig.maxCalls == 4 ? 0 : 1;
}
CPP

read -r -a pjsip_cflags <<<"$(pkg-config --cflags "${PKG_NAME}")"
read -r -a pjsip_libs <<<"$(pkg-config --static --libs "${PKG_NAME}")"

"${CXX:-c++}" \
    -std=c++20 \
    -Wall \
    -Wextra \
    -Wpedantic \
    "${pjsip_cflags[@]}" \
    "${work_dir}/pjsua2-smoke.cpp" \
    -o "${work_dir}/pjsua2-smoke" \
    "${pjsip_libs[@]}"

"${work_dir}/pjsua2-smoke"

printf 'PJSUA2 compile/link smoke test: PASS\n'
printf 'PJSIP dependency verification: PASS\n'
