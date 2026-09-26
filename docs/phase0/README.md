# Phase 0 — Repository & Engineering Environment

Phase 0 is complete only when the project builds, unit tests pass, clang-tidy passes,
ASan/UBSan pass, Git history is clean, and GitHub Actions is green.

## What you are proving

You are not proving VoIP knowledge yet. You are proving that you can create a professional
C++ engineering environment before feature development starts.

The required Phase 0 stack is:

- Git
- CMake
- C++20
- clang-format
- clang-tidy
- GoogleTest
- AddressSanitizer (ASan)
- UndefinedBehaviorSanitizer (UBSan)
- GitHub Actions

## Step 1 — Inspect the repository before running anything

Read these files in this order:

1. `CMakeLists.txt`
2. `src/CMakeLists.txt`
3. `tests/CMakeLists.txt`
4. `cmake/CompilerWarnings.cmake`
5. `cmake/Sanitizers.cmake`
6. `cmake/StaticAnalysis.cmake`
7. `cmake/Dependencies.cmake`
8. `.clang-format`
9. `.clang-tidy`
10. `.github/workflows/*.yml`

You should be able to explain what each file contributes to the build pipeline.

## Step 2 — Verify/install the toolchain

If you already installed the tools, first check them manually:

```bash
git --version
cmake --version
g++ --version
clang --version
clang-format --version
clang-tidy --version
gdb --version
ninja --version
```

If one is missing, run:

```bash
./scripts/bootstrap.sh
```

Do not treat the bootstrap script as a black box. Open it and understand every package it
installs.

## Step 3 — Build using the exact commands from the roadmap

From the repository root:

```bash
cmake -S . -B build \
    -DCMAKE_BUILD_TYPE=Debug

cmake --build build -j

ctest --test-dir build
```

Expected result: configuration succeeds, compilation succeeds, and all unit tests pass.

GoogleTest is provided by the Ubuntu `libgtest-dev` package installed by `scripts/bootstrap.sh`.

Run the executable:

```bash
./build/src/linux-voip-softphone
```

Expected output contains:

```text
linux-voip-softphone
Phase 0 - Repository & Engineering Environment
Standard: C++20
Status: project skeleton is ready
```

## Step 4 — Repeat the build using CMake Presets

Presets make developer and CI builds reproducible:

```bash
cmake --preset debug
cmake --build --preset debug -j
ctest --preset debug
```

Understand the relationship among `CMakePresets.json`, the generated build directory, and the
root `CMakeLists.txt`.

## Step 5 — Learn and verify clang-format

Check formatting without changing files:

```bash
find src tests -type f \( -name '*.cpp' -o -name '*.hpp' \) \
    -print0 | xargs -0 clang-format --dry-run --Werror
```

Then intentionally change indentation in one `.cpp` file and run:

```bash
clang-format -i src/app/main.cpp
```

Inspect the diff:

```bash
git diff
```

The point is to understand that formatting is deterministic and tool-enforced.

## Step 6 — Run clang-tidy

```bash
./scripts/run_clang_tidy.sh
```

`ENABLE_CLANG_TIDY=ON` causes CMake to run clang-tidy as part of compilation.

You should understand the difference:

- compiler warnings: diagnostics from the compiler
- clang-format: source-code formatting
- clang-tidy: static analysis and modernization checks

## Step 7 — Run ASan and UBSan

```bash
./scripts/run_sanitizers.sh
```

This configures a separate build with:

```text
-fsanitize=address,undefined
-fno-omit-frame-pointer
```

Phase 0 should finish with no sanitizer report.

You should know what the tools are intended to detect:

- ASan: invalid memory access, use-after-free, buffer overflows, related memory errors
- UBSan: selected forms of undefined behavior

## Step 8 — Understand GoogleTest

Open:

```text
tests/unit/test_build_info.cpp
```

Then run:

```bash
ctest --preset debug --output-on-failure
```

Also list discovered tests:

```bash
ctest --test-dir build/debug -N
```

You should be able to explain why tests are separate targets and why application code is placed
in the `voip_core` library instead of testing `main()` directly.

## Step 9 — Collect recruiter evidence

Run:

```bash
./scripts/collect_phase0_evidence.sh
```

It generates files under:

```text
evidence/00-phase0/
```

Keep useful evidence such as:

```text
toolchain.txt
build.log
test.log
sanitizers.log
clang-tidy.log
```

Before committing evidence, inspect it for machine-specific paths or information you do not want
in a public repository.

## Step 10 — Initialize Git and create the Phase 0 milestone

If this directory is not already a Git repository:

```bash
git init
git branch -M main
```

Inspect before committing:

```bash
git status
git diff
```

Create the initial engineering commit:

```bash
git add .
git commit -m "chore: bootstrap C++20 project skeleton"
```

Only after build, tests, clang-tidy, and sanitizers are passing, create the roadmap tag:

```bash
git tag -a v0.1-project-skeleton \
    -m "Phase 0: repository and engineering environment"
```

Verify:

```bash
git log --oneline --decorate --graph --all
git tag --list
```

## Step 11 — Push to GitHub and verify CI

Create an empty GitHub repository named `linux-voip-softphone`, then add it as your remote and
push `main` plus the milestone tag.

After pushing, verify that these workflows are green:

```text
Build
Tests and Sanitizers
Static Analysis
```

Do not claim Phase 0 complete while CI is red.

## Step 12 — Update the root README status

When everything above is genuinely passing, the root README should truthfully show:

```text
Build: Passing
Tests: Passing
Compiler: GCC / Clang
Standard: C++20
Platform: Ubuntu
```

## Phase 0 Definition of Done

- [ ] `cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug` succeeds
- [ ] `cmake --build build -j` succeeds
- [ ] `ctest --test-dir build` passes
- [ ] Debug preset passes
- [ ] clang-format check passes
- [ ] clang-tidy passes
- [ ] ASan + UBSan tests pass
- [ ] Git repository has a meaningful initial commit
- [ ] Git tag `v0.1-project-skeleton` exists
- [ ] GitHub Actions Build is green
- [ ] GitHub Actions Tests and Sanitizers is green
- [ ] GitHub Actions Static Analysis is green
- [ ] Phase 0 evidence has been inspected and saved
- [ ] You can explain every tool in the Phase 0 toolchain without reading the README

## Interview proof

A concise explanation you should eventually be able to give in your own words:

> Tôi thiết lập project với CMake, compiler warnings, static analysis và sanitizer ngay từ đầu để
> phát hiện lỗi sớm thay vì chỉ debug khi runtime.

Do not memorize that sentence mechanically. Be prepared to open the repository and demonstrate
which files implement each part of that statement.
