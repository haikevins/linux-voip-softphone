#!/usr/bin/env bash
set -euo pipefail

sudo apt update
sudo apt install -y \
    build-essential \
    cmake \
    ninja-build \
    git \
    gdb \
    clang \
    clang-format \
    clang-tidy \
    pkg-config \
    libgtest-dev \
    ca-certificates

echo
echo "Project toolchain versions:"
git --version
cmake --version | head -n 1
c++ --version | head -n 1
clang --version | head -n 1
clang-format --version
clang-tidy --version | head -n 1
