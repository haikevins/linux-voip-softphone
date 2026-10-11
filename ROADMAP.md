# Linux VoIP Softphone Roadmap

This roadmap defines the project sequence, milestone boundaries, and acceptance intent. Detailed implementation, architecture, protocol notes, and retained evidence live in `docs/` and `evidence/`; this file does not duplicate them.

## Project goal

Build a Linux desktop SIP softphone while demonstrating practical C++20, Linux systems programming, TCP/UDP networking, SIP/RTP, PJSIP/PJSUA2, Qt/QML, multithreading, debugging, testing, and engineering discipline.

The repository separates focused learning work from production work:

```text
labs/   small programs that isolate a concept
src/    production code used by the softphone
```

A phase is complete only when its required code, tests, evidence, and explanation exist.

## Current progress

| Phase | Topic | Status | Milestone |
|---|---|---|---|
| 0 | Repository & engineering environment | ACCEPTED | `v0.1-project-skeleton` |
| 1 | Modern C++ foundation | ACCEPTED | `v0.2-cpp-core` |
| 2 | Linux system programming & IPC | ACCEPTED | `v0.3-linux-system` |
| 3 | TCP/UDP networking | ACCEPTED | `v0.4-networking` |
| 4 | SIP + Asterisk lab | ACCEPTED | `v0.5-sip-asterisk` |
| 5 | PJSIP CLI softphone | ACCEPTED | `v0.6-pjsip-cli` |
| 6 | RTP + Audio | ACCEPTED | `v0.7-rtp-audio` |
| 7 | Qt/QML GUI | ACCEPTED | `v0.8-qt-gui` |
| 8 | Multithreading | MERGED; automated verification PASS; final maintainer acceptance PENDING | no separate Phase 8 tag |
| 9 | Debugging, memory & core dump | PLANNED after explicit Phase 8 acceptance | `v0.9-diagnostics` |
| 10 | RTSP mini-lab | PLANNED | `v0.10-rtsp-lab` |
| 11 | Final engineering release | PLANNED | `v1.0.0` |

Phase 8 keeps the following safety boundary: live external callback quiescence is **UNPROVEN**, production-client real `PJ_EBUSY` recovery is **UNPROVEN**, and PJSUA2 integration readiness is **BLOCKED**. These results are not converted into PASS by documentation cleanup.

## Phase 0 — Repository & engineering environment

**Goal:** establish a reproducible C++20 project before adding VoIP behavior.

**Core deliverables:**

- Git/CMake project layout;
- compiler warnings and formatting;
- clang-tidy;
- GoogleTest;
- ASan/UBSan;
- GitHub Actions.

**Acceptance:** build, tests, quality checks, documentation, and retained verification evidence pass for the recorded environment.

**Milestone:** `v0.1-project-skeleton`.

Primary guide: `docs/phase0/README.md`.

## Phase 1 — Modern C++ foundation

**Goal:** establish the ownership and type-safety principles used by later production code.

**Core topics:**

- RAII and file-descriptor/resource ownership;
- smart pointers and move semantics;
- const correctness and `enum class`;
- STL containers and value semantics;
- explicit call-state behavior.

**Deliverables:** focused C++ labs, production call-state/core types, unit tests, and architecture explanation.

**Milestone:** `v0.2-cpp-core`.

Primary guide: `docs/phase1/README.md`.

## Phase 2 — Linux system programming & IPC

**Goal:** work directly with Linux process and IPC primitives.

**Core topics:**

- process lifecycle;
- `fork()` / `exec()`;
- signals and `sigaction()`;
- file descriptors and RAII;
- Unix-domain sockets.

**Deliverables:** Linux labs plus process/IPC integration work with automated and retained runtime verification.

**Milestone:** `v0.3-linux-system`.

Primary guide: `docs/phase2/README.md`.

## Phase 3 — TCP/UDP networking

**Goal:** understand transport behavior before using a SIP stack.

**Core topics:**

- IPv4 endpoints;
- TCP client/server;
- UDP datagrams;
- packet ordering/loss concepts;
- latency, jitter, and throughput foundations.

**Deliverables:** reusable socket abstractions, integration tests, and networking documentation.

**Milestone:** `v0.4-networking`.

Primary guide: `docs/phase3/README.md`.

## Phase 4 — SIP + Asterisk lab

**Goal:** verify SIP signaling independently of the custom softphone.

**Core flow:**

```text
REGISTER -> challenge -> authenticated REGISTER -> 200 OK
INVITE -> 100 Trying -> 180 Ringing -> 200 OK -> ACK
RTP
BYE -> 200 OK
```

**Deliverables:**

- local Asterisk configuration with lab endpoints;
- registration and call-flow verification;
- sanitized evidence;
- SIP protocol and architecture documentation.

Credentials and authentication-bearing raw traces must not be committed.

**Milestone:** `v0.5-sip-asterisk`.

Primary guide: `docs/phase4/README.md`.

## Phase 5 — PJSIP CLI softphone

**Goal:** integrate PJPROJECT/PJSUA2 without Qt first.

**Accepted scope:**

- account registration;
- outgoing call lifecycle;
- incoming call lifecycle;
- answer/hangup behavior;
- configuration and failure handling;
- automated regression and live lab verification.

The CLI milestone separates SIP/PJSUA2 behavior from GUI concerns so signaling can be tested independently.

**Milestone:** `v0.6-pjsip-cli`.

Primary guide: `docs/phase5/README.md`.

## Phase 6 — RTP + Audio

**Status:** accepted for the documented local lab.

**Goal:** prove real two-way Linux audio and expose useful media diagnostics.

**Accepted scope:**

- ThinkPad ↔ peer two-way audio;
- RTP RX/TX activity;
- codec/media observations;
- packet-loss, jitter, and RTCP-related diagnostics;
- focused Wireshark analysis retained as sanitized evidence.

The raw packet capture containing audio payload is intentionally kept outside Git.

**Milestone:** annotated tag `v0.7-rtp-audio`.

Primary guide: `docs/phase6/README.md`.

## Phase 7 — Qt/QML GUI

**Status:** accepted and merged to `main`.

**Goal:** put a Qt/QML interface over the already verified signaling/media path without making QML or the UI thread own PJSUA2.

**Accepted scope:**

- Qt/QML shell;
- SIP registration;
- outgoing and incoming calls;
- Answer / Decline;
- local and remote hangup;
- remote cancel before answer;
- connected-call duration;
- explicit SIP `603 Decline` on the softphone-to-Asterisk leg;
- live codec/RX/TX/loss/jitter/RTT display;
- registration reuse across sequential calls;
- Qt-only, Qt/PJSIP, and inherited Phase 5/6 regression verification.

Multiple accounts, call waiting, mute/hold, audio-device selection UI, and persistent call history remain outside the Phase 7 acceptance boundary.

**Milestone:** annotated tag `v0.8-qt-gui`.

Primary guide: `docs/phase7/README.md`.

## Phase 8 — Multithreading

**Current status:** merged to `main`; post-merge automated verification PASS; final maintainer acceptance **PENDING**.

**Goal:** make concurrency behavior explicit, testable, and safe to reason about.

**Implemented scope:**

- `ThreadSafeQueue<T>`;
- owned `SerialTaskWorker`;
- deterministic queue/worker tests;
- repeated 100,000-event MPMC stress;
- Clang 18 ThreadSanitizer verification;
- Qt/PJSUA2 ownership and callback-lifetime audits;
- shutdown-order/failure-path hardening;
- controlled live PJSUA2 lifecycle evidence.

The reusable queue/worker is verified independently; it is not retroactively claimed to be the deployed transport for every production callback path.

**Safety boundary that remains in force:**

- controlled PJSUA2 `PJ_EBUSY` recovery: observed/evidenced in the recorded controlled probe;
- late registration callbacks after `shutdown2`: none observed in that recorded run;
- live external callback quiescence: **UNPROVEN**;
- production-client real `PJ_EBUSY` recovery: **UNPROVEN**;
- PJSUA2 integration readiness: **BLOCKED**.

There is no separate Phase 8 release tag in the current roadmap. Phase 9 starts only after explicit maintainer acceptance.

Primary guide: `docs/phase8/README.md`. Evidence index: `evidence/phase8/README.md`.

## Phase 9 — Debugging, memory & core dump

**Status:** planned after Phase 8 acceptance.

**Goal:** demonstrate repeatable native debugging rather than only fixing ad-hoc crashes.

**Planned work:**

- isolated crash generator or lab-only fault;
- core-dump generation and GDB backtrace analysis;
- deliberate memory-lifetime defect in non-production test/lab scope;
- AddressSanitizer diagnosis;
- documented root cause, fix, and re-verification;
- structured logging/diagnostic improvements where useful.

**Milestone:** `v0.9-diagnostics`.

## Phase 10 — RTSP mini-lab

**Status:** planned.

**Goal:** learn RTSP without forcing it into the SIP softphone architecture.

**Planned scope:**

- `OPTIONS`;
- `DESCRIBE`;
- SDP inspection;
- `SETUP`;
- `PLAY`;
- explanation of SIP vs RTSP vs RTP roles.

**Milestone:** `v0.10-rtsp-lab`.

## Phase 11 — Final engineering release

**Status:** planned.

**Goal:** consolidate the accepted work into a polished engineering release.

Expected final capabilities include the accepted SIP/audio/GUI path plus the later diagnostics work, final testing/refactoring, documentation cleanup, and a reproducible demo.

**Milestone:** `v1.0.0`.

## Evidence, release rules, and Definition of Done

For each phase, prefer:

```text
source code
    -> automated tests
    -> runtime evidence
    -> written explanation
```

Retain only sanitized evidence that materially proves a result. Do not commit SIP passwords, live Digest response material, SRTP inline keys, raw secret-bearing traces, or unrelated generated build output.

Git tags identify published milestones; Git refs are authoritative for whether a milestone was actually published. Do not invent a tag merely to make numbering look symmetric.

## Documentation map

- `docs/phaseN/README.md` — phase objective, scope, verification, evidence, and boundary.
- `docs/architecture/` — reusable system/component architecture.
- `docs/protocols/` — SIP/RTP and later protocol behavior.
- `docs/interview/` — phase review questions.
- `evidence/phaseN/README.md` — evidence index and provenance.
- `scripts/phaseN/` and `scripts/quality/` — reproducible verification workflows where present.

## Next step

Phase 8 has not yet received final maintainer acceptance. Until that explicit sign-off occurs, the repository remains at the Phase 8 closure boundary and Phase 9 must not be treated as started.
