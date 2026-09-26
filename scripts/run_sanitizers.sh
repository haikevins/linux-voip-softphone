#!/usr/bin/env bash
set -euo pipefail

cmake --preset sanitizers
cmake --build --preset sanitizers -j
ctest --preset sanitizers
