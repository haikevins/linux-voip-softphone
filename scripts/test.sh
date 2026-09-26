#!/usr/bin/env bash
set -euo pipefail

cmake --preset debug
cmake --build --preset debug -j
ctest --preset debug
