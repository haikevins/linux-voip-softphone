# Phase 8 — Final runtime acceptance (ThinkPad execution required)

**Scope:** Final user-visible GUI responsiveness and repeatable *offline* regression after Phase 8 multithreading roadmap closure. This test is a **candidate**, not an assertion that the app is fully operational or that Phase 8 has been signed off.

Baseline: branch `phase/8-multithreading`, ancestor commit `0cd1a7bdb6b0de8e9ed6d1efef0d9722309436a7` (roadmap closure). No production source, QML, CMake target, or existing CTest registration is changed by this overlay. The pre-existing canonical suite should remain **147** tests.

## What was actually inspected

- `src/app/qt_gui_main.cpp`: `--smoke-test` loads `qrc:/gui/Main.qml`, checks two QML objects / disabled backend, then quits after ~100ms; this is a startup/exit test, **not** a long GUI usability test.
- `src/gui/Main.qml`: a preview keypad (0–9, *, #), destination input, Delete/Clear, Register, Call, Hang up, RTP panel. When not started using `--sip-register`, SIP controls are disabled.
- `src/gui/QtRegistrationBackend.cpp`, `tests/qt/test_qt_registration_backend.cpp`: injected fake session exercises worker/QObject event handoff; **not** real SIP registration.
- `tests/phase8/test_mpmc_stress.cpp`: 4 producers × 25,000 events = 100,000 events, 4 consumers.
- `scripts/phase8/verify-tsan.sh`: standalone 6 C++ tests × 3 repeats using Clang18 TSan, **not** GUI/Qt/PJSUA2 runtime under TSan.
- `scripts/phase8/verify-roadmap-exit.sh`: source/evidence inventory that must continue to report PJSUA2 shutdown integration **BLOCKED**.
- `README_roadmap.md` section 11 and section 23: Phase 8 multithreading requirements and Code + Test + Evidence + Documentation definition of done.

## A. Automatic, offline, on the real ThinkPad

Prerequisites: installed `cmake`, `ninja`, `ctest`, `clang++-18`, Qt6/PJSIP build dependencies, Python3, `git`, and a branch descendant of the closure commit. Tests run using existing local build directories; only generated `build/` files and a unique `/tmp/phase8-runtime-acceptance.*` directory are written.

```bash
cd ~/Documents/linux-voip-softphone
bash scripts/phase8/verify-final-runtime-acceptance.sh --automatic
```

The script performs the following *actual runtime and test* gates:

| Gate | Evidence | Acceptance |
|---|---|---|
| Canonical configure/build | `configure.log`, `build.log` | Qt GUI + PJSIP enabled, executable exists |
| Canonical CTest | `canonical.log` | **147/147 PASS** |
| Qt fake-session worker/event handoff | `qt-worker-repeat.log` | **10 repeat runs PASS** |
| Phase 8 focused | `phase8-focused.log` | **12/12 PASS** |
| 100k-event MPMC | `mpmc-stress.log` | **30 repetitions PASS** (configurable 1–100) |
| TSan six tests × 3 | `tsan.log` | **18/18 PASS** using clang++-18 |
| PJSIP *non-live* contract | `pjsip-nonlive.log` | **45/45 PASS**, no network calls |
| Closure/source lifetime audit | `roadmap-audit.log` | PASS while integration remains **BLOCKED** |
| Probe fail-detection unit tests | `harness-unit.log` | **6/6 PASS** |
| GUI startup/exit soak | `gui-smoke.log`, `gui-smoke-results.json` | **40/40 PASS**, each exit=0, each <=8 seconds, <=4 seconds considered acceptable startup |

Output prints `EVIDENCE_DIR=/tmp/phase8-runtime-acceptance.XXXXXXXX`. Keep this exact directory for the manual test. GUI smoke sets `QT_QPA_PLATFORM=offscreen` and software rendering only for that subprocess, without credentials or network. The GUI itself remains in preview mode.

**Performance caveat:** Startup time includes loading Qt and the fixed 100ms smoke-test window. A 4-second *per-process* threshold protects against gross startup slowdown, but is not a UI frame-time/FPS specification. A slow run is a meaningful FAIL to examine (system load, graphics driver, I/O), not automatic proof of an application regression. Values can be adjusted only with documented justification using `PHASE8_ACCEPT_GUI_SLOW_MS`, `PHASE8_ACCEPT_GUI_TIMEOUT`, and `PHASE8_ACCEPT_GUI_REPEATS`. Default thresholds are conservative.

## B. Human-operated GUI endurance / normal window closure

Run from a **graphical desktop terminal**, not SSH/headless. Paste the exact `EVIDENCE_DIR` from stage A:

```bash
cd ~/Documents/linux-voip-softphone
PHASE8_ACCEPT_LOG_DIR='/tmp/phase8-runtime-acceptance.YOUR_REAL_DIR' \
    bash scripts/phase8/verify-final-runtime-acceptance.sh --manual
```

The harness starts the actual `build/qt-pjsip-gui-debug/src/voip-qt-gui` **without** `--sip-register`, monitors `/proc/<pid>/status` and `/proc/<pid>/fd` every 0.5s, then requires your explicit typed `yes` and a **normal window X-close** exit code 0. Default minimum observation 90 seconds and maximum close wait 45 seconds. Use `PHASE8_MANUAL_OBSERVE_SECONDS=180` for a longer session.

During the full observation period, verify:

1. Window appears and paints normally; move, resize, scroll, minimize and restore if supported by your desktop.
2. Click all keypad digits, `*`, `#`; destination changes correctly; Delete removes one character; Clear empties input. Type a 64-character extension and try deleting/retyping.
3. Repeat input/clear/resize for 90 seconds (or 180); no sustained freeze, noticeable >1 second UI stalls, repaint failure or unexpected crash.
4. In **preview/no-SIP mode**, Register, Call and Hang up stay disabled even if destination is filled. The interface must **not** pretend that a SIP account is registered.
5. Respond `yes` **only** if all the checks passed; then close the GUI using the window close control, *not* `kill`, SIGTERM, or Ctrl+C.

`gui-manual-samples.jsonl` records RSS KiB, thread count, and FD count per sample. `gui-manual-results.json` contains min/max/first/last observations, plus user acceptance. Resource numbers are *observations*, not leak proof: use their trend to investigate unexpected continuous growth, and fail the manual check if concerning behavior is observed. No automatic memory-leak certification is claimed.

## C. Result and exit decision

**Automated PASS ≠ final acceptance.** Require both:

- `PASS: PHASE 8 AUTOMATED FINAL RUNTIME ACCEPTANCE` and `acceptance-status.txt` containing `AUTOMATED: PASS`.
- `PASS: Phase 8 manual GUI runtime gate (no SIP, human confirmed)` and `gui-manual-results.json` with `accepted: true`.

If either fails, keep Phase 8 **NOT ACCEPTED**; retain the `/tmp/` log directory and investigate. Do not commit logs as if already verified. A passing test supports acceptance of the *offline Phase 8 multithreading/GUI preview scope* only, subject to final maintainer approval.

**Not covered:** live SIP registration/call audio quality, PJSUA2 `shutdown2` recovery on `PJ_EBUSY`, external callback quiescence, actual crash-free teardown of registered PJSUA2 objects, sustained FPS/rendering benchmark, real-world power/battery performance. A user with a controlled Asterisk lab can separately exercise SIP, but this test **deliberately** does not start calls or handle credentials while known lifetime safety risks are unresolved.

Report the console transcript and the paths to `acceptance-status.txt`, `gui-smoke-results.json`, `gui-manual-results.json`; do not share passwords, SIP config secrets, or live registration traces. **No automatic git commit/push/merge or change of roadmap sign-off** is performed by these scripts.


## THINKPAD VERIFIED FINAL RUNTIME ACCEPTANCE

The following results were verified from actual ThinkPad logs.
The detailed record is in `evidence/07-threading/final-runtime-acceptance.txt`.

Automated acceptance: PASS (verified resume).

Manual GUI acceptance: PASS (human confirmed).

Normal X-close: PASS, exit code 0.

Live SIP/PJSUA2 shutdown safety: NOT VERIFIED.

Final Phase 8 maintainer sign-off: PENDING.
