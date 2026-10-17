# Contributing

Thanks for your interest in contributing to Linux VoIP Softphone.

This repository is maintained as an engineering portfolio and learning project. Contributions should stay focused, reviewable, and consistent with the existing build and verification workflow.

## Development workflow

1. Start from an up-to-date `main` branch and create a focused topic branch.
2. Keep each commit small, buildable, and limited to one logical change.
3. Run the relevant build, tests, formatting, and static-analysis checks before opening a pull request.
4. Do not include generated build trees, credentials, raw SIP authentication material, packet captures, or unrelated local artifacts.
5. Open a pull request and wait for all required GitHub Actions checks to pass before merge.

## Build

The project uses CMake presets. For the complete Qt/PJSIP release configuration:

```bash
export PKG_CONFIG_PATH="$HOME/.local/pjproject-2.17/lib/pkgconfig${PKG_CONFIG_PATH:+:$PKG_CONFIG_PATH}"

cmake --preset release
cmake --build --preset release -j"$(nproc)"
```

For development, use the preset appropriate to the area being changed. Existing presets and phase documentation under `docs/` describe the supported configurations.

## Tests and verification

Run the tests that cover the modified area. Before proposing a repository-wide change, the final release verification entry point is:

```bash
export PKG_CONFIG_PATH="$HOME/.local/pjproject-2.17/lib/pkgconfig${PKG_CONFIG_PATH:+:$PKG_CONFIG_PATH}"

./scripts/phase11/verify.sh
```

Documentation-only changes should also pass:

```bash
git diff --check
bash scripts/quality/verify-docs-structure.sh
bash scripts/quality/verify-evidence-structure.sh
```

## Formatting and static analysis

Keep C++ changes consistent with the repository's existing clang-format and clang-tidy checks. Formatting or static-analysis fixes belong in the same commit that introduces the affected code rather than in a later cleanup commit.

## Branches and commits

Use short, descriptive branch names such as:

```text
feat/...
fix/...
docs/...
test/...
```

Use concise conventional-style commit subjects, for example:

```text
feat(sip): ...
fix(rtp): ...
test(gui): ...
docs: ...
build: ...
```

Avoid unrelated changes in the same commit and do not rewrite published release tags.

## Pull requests

A pull request should explain what changed, why it changed, and how it was verified. Keep scope narrow enough that the change can be reviewed independently.

All required CI checks should be green before merge.
