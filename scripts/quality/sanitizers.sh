#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
cd "${ROOT_DIR}"

cmake --preset sanitizers
cmake --build --preset sanitizers -j
ctest --preset sanitizers
