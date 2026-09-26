#!/usr/bin/env bash
set -euo pipefail

cmake --preset clang-tidy
cmake --build --preset clang-tidy --clean-first -j
