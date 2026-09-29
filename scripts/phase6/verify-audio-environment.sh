#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
MIN_PJSIP_VERSION="2.17"
PKG_NAME="libpjproject"
CXX_BIN="${CXX:-c++}"

fail() {
    printf 'ERROR: %s\n' "$*" >&2
    exit 1
}

require_command() {
    local command_name="$1"
    command -v "${command_name}" >/dev/null 2>&1 || \
        fail "required command not found: ${command_name}"
}

require_command pkg-config
require_command "${CXX_BIN}"

if ! pkg-config --exists "${PKG_NAME} >= ${MIN_PJSIP_VERSION}"; then
    printf 'Audio environment verification: FAIL\n' >&2
    printf 'Required pkg-config module: %s >= %s\n' "${PKG_NAME}" "${MIN_PJSIP_VERSION}" >&2
    printf 'PKG_CONFIG_PATH: %s\n' "${PKG_CONFIG_PATH:-<unset>}" >&2
    exit 1
fi

version="$(pkg-config --modversion "${PKG_NAME}")"
prefix="$(pkg-config --variable=prefix "${PKG_NAME}")"

printf 'Repository root: %s\n' "${ROOT_DIR}"
printf 'PJSIP version: %s\n' "${version}"
printf 'PJSIP prefix: %s\n' "${prefix}"

work_dir="$(mktemp -d)"
trap 'rm -rf "${work_dir}"' EXIT

cat >"${work_dir}/audio-device-preflight.cpp" <<'CPP'
#include <pjsua2.hpp>

#include <iostream>
#include <string>

int main() {
    pj::Endpoint endpoint;
    bool library_created = false;

    try {
        endpoint.libCreate();
        library_created = true;

        pj::EpConfig config;
        config.logConfig.level = 3;
        config.logConfig.consoleLevel = 3;
        endpoint.libInit(config);
        endpoint.libStart();

        const auto devices = endpoint.audDevManager().enumDev2();

        bool has_capture = false;
        bool has_playback = false;

        std::cout << "Audio device count: " << devices.size() << '\n';

        for (const auto& device : devices) {
            const bool capture_capable = device.inputCount > 0;
            const bool playback_capable = device.outputCount > 0;

            has_capture = has_capture || capture_capable;
            has_playback = has_playback || playback_capable;

            std::cout << "- id=" << device.id
                      << " driver=" << device.driver
                      << " name=" << device.name
                      << " inputs=" << device.inputCount
                      << " outputs=" << device.outputCount
                      << " default_rate=" << device.defaultSamplesPerSec
                      << '\n';
        }

        endpoint.libDestroy();
        library_created = false;

        if (!has_capture) {
            std::cerr << "Audio device preflight: FAIL (no capture-capable device)\n";
            return 2;
        }

        if (!has_playback) {
            std::cerr << "Audio device preflight: FAIL (no playback-capable device)\n";
            return 3;
        }

        std::cout << "Capture-capable audio device: PASS\n";
        std::cout << "Playback-capable audio device: PASS\n";
        std::cout << "Audio device preflight: PASS\n";
        return 0;
    } catch (const pj::Error& error) {
        if (library_created) {
            try {
                endpoint.libDestroy();
            } catch (...) {
            }
        }

        std::cerr << "Audio device preflight: FAIL\n";
        std::cerr << error.info() << '\n';
        return 1;
    }
}
CPP

read -r -a pjsip_cflags <<<"$(pkg-config --cflags "${PKG_NAME}")"
read -r -a pjsip_libs <<<"$(pkg-config --static --libs "${PKG_NAME}")"

printf '\n== PJSUA2 audio-device compile/link smoke test ==\n'
"${CXX_BIN}" \
    -std=c++20 \
    -Wall \
    -Wextra \
    -Wpedantic \
    "${pjsip_cflags[@]}" \
    "${work_dir}/audio-device-preflight.cpp" \
    -o "${work_dir}/audio-device-preflight" \
    "${pjsip_libs[@]}"

printf 'PJSUA2 audio-device compile/link smoke test: PASS\n'
printf '\n== PJSUA2 audio-device enumeration ==\n'
"${work_dir}/audio-device-preflight"

printf '\nAudio environment verification: PASS\n'
