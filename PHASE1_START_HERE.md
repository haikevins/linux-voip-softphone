# Phase 1 package — Start here

This archive contains the **target Phase 1 project state** for the existing
`linux-voip-softphone` repository.

Your detailed tutorial is:

```text
docs/phase1/README.md
```

If you already completed Phase 0 in `~/Documents/linux-voip-softphone`, do not delete its `.git`
directory. Extract this archive to a temporary location and use the files as the Phase 1 target
while implementing/applying the changes on your existing Git working tree.

Recommended:

```bash
mkdir -p ~/Documents/phase1-package
unzip voip-softphone.zip -d ~/Documents/phase1-package
less ~/Documents/phase1-package/voip-softphone/docs/phase1/README.md
```

Then work in your real repository:

```bash
cd ~/Documents/linux-voip-softphone
git switch -c phase/1-modern-cpp
```

The package intentionally contains no `.git` directory and no pre-generated Phase 1 evidence logs.
Evidence must be generated on your own Ubuntu machine.
