# Phase 5 — PJSIP CLI Softphone

## Status

**Completed** — milestone tag `v0.6-pjsip-cli`.

Phase 5 introduces project-owned PJSUA2 command-line clients for registration, outgoing calls, and incoming calls while deliberately keeping real audio-device handling out of scope.

## Objective

Integrate PJPROJECT/PJSIP with the C++20 project, implement deterministic SIP account/call lifecycles, validate them against the Phase 4 Asterisk lab, and automate as much verification as possible without committing credentials or sensitive SIP traces.

## Scope

Included in this phase:

- PJPROJECT/PJSIP dependency detection through `pkg-config`;
- PJSUA2 build integration behind `ENABLE_PJSIP`;
- endpoint and account configuration abstractions;
- SIP registration CLI;
- one-shot outgoing call CLI;
- one-shot incoming call/auto-answer CLI;
- callback synchronization and bounded waits;
- deterministic call/account/endpoint teardown;
- process-level CLI contract verification;
- live Asterisk registration/call checks;
- scrubbed Phase 5 evidence.

Deliberately excluded:

- real capture/playback device routing;
- application-owned media connections;
- RTP statistics and audio diagnostics;
- Qt/QML.

## Deliverables

| Area | Purpose | Key files |
|---|---|---|
| Dependency integration | Detect/link PJPROJECT 2.17+ | `cmake/Dependencies.cmake`, `CMakePresets.json` |
| Endpoint config | Map transport/listen settings to PJSUA2 | `src/pjsip/SipEndpointConfig.*` |
| Account config | Build identity/registrar/auth configuration | `src/pjsip/SipAccountConfig.*` |
| Registration CLI | Register one account and report final state | `voip-sip-register` sources |
| Outgoing-call CLI | Register, call, hold, hang up, exit | `voip-sip-call` sources |
| Incoming-call CLI | Register, ring, auto-answer, hold, hang up, exit | `voip-sip-listen` sources |
| Verification | Exercise dependency, tests, sanitizers, tidy, regressions | `scripts/phase5/verify.sh` |
| Architecture note | Explain PJSUA2 lifecycle/callback/teardown design | `docs/architecture/pjsip-cli.md` |
| Interview review | Capture Phase 5 review questions | `docs/interview/phase5-questions.md` |

## Architecture and design

The PJSIP-enabled path is intentionally layered:

```text
CLI argument parsing
        |
        v
SipEndpointConfig + SipAccountConfig
        |
        v
registration / outgoing / incoming client
        |
        v
PJSUA2 Endpoint + Account + Call
        |
        v
Asterisk lab
```

The common endpoint lifecycle is based on PJSUA2 initialization, transport creation, library startup, account creation, asynchronous callbacks, and explicit shutdown ordering.

Important design rules:

- SIP passwords come from an environment variable, not command-line arguments;
- callback-updated state is synchronized before the CLI thread reads it;
- timeout paths are bounded instead of waiting forever;
- call objects are allowed to reach a disconnected state before account shutdown;
- account shutdown is performed before destroying the endpoint/library;
- client teardown avoids forced object deletion and PJSUA2 busy-state errors;
- the call clients use the null audio device so Phase 5 proves signaling/lifecycle without claiming finished media support.

See `docs/architecture/pjsip-cli.md` for the full callback and lifetime model.

## Build and run

The Phase 5 workstation uses PJPROJECT 2.17 installed under a user-local prefix. Expose its pkg-config metadata before configuring:

```bash
export PKG_CONFIG_PATH="$HOME/.local/pjproject-2.17/lib/pkgconfig${PKG_CONFIG_PATH:+:$PKG_CONFIG_PATH}"
```

Verify the dependency and build the PJSIP preset:

```bash
./scripts/phase5/verify-pjsip-dependency.sh
cmake --preset pjsip-debug
cmake --build --preset pjsip-debug -j
```

The build produces:

```text
voip-sip-register
voip-sip-call
voip-sip-listen
```

Use each program's `--help` output for the authoritative CLI contract. Password values must be provided through the configured environment variable and must not be copied into documentation or evidence.

## Verification

Run the complete automated Phase 5 verifier:

```bash
./scripts/phase5/verify.sh
```

The verifier covers:

- repository/diff scope checks;
- formatting;
- PJPROJECT/PJSIP dependency compile/link smoke test;
- PJSIP debug build;
- PJSIP-labeled tests;
- full PJSIP regression;
- ASan and UBSan with PJSIP enabled;
- `clang-tidy` with PJSIP enabled;
- non-PJSIP regression.

`PjsipCliContractIntegrationTest` verifies the three executable interfaces without a live SIP server. Live registration/outgoing/incoming tests remain explicit manual lab checks because they depend on Asterisk, LAN endpoints, and credentials.

## Evidence

Committed Phase 5 evidence lives under:

```text
evidence/phase5/
```

It records result/state summaries for:

- automated verification;
- live registration;
- live outgoing call;
- live incoming call;
- phase completion.

Raw SIP logger output, Digest authorization fields, SIP passwords, and SDP keying material are intentionally excluded.

## Interview focus

Use `docs/interview/phase5-questions.md` with the implementation. Be ready to explain:

- why PJSUA2 is integrated behind an explicit CMake option;
- `pkg-config` and static-link dependency propagation;
- endpoint, account, and call object responsibilities;
- synchronous CLI orchestration around asynchronous PJSUA2 callbacks;
- registration and call state callbacks;
- incoming-call object creation from a PJSUA2 call ID;
- why teardown order matters;
- why the CLI reads credentials from an environment variable;
- the difference between process-level contract tests and live SIP integration;
- why negotiated RTP does not yet prove audible two-way application audio.

## Exit criteria

Phase 5 is complete when all of the following statements are true:

- PJPROJECT/PJSIP dependency verification passes;
- registration works against the Phase 4 Asterisk lab;
- outgoing calls reach confirmed/disconnected states cleanly;
- incoming calls can ring, answer, confirm, and disconnect cleanly;
- automated PJSIP and non-PJSIP regressions pass;
- sanitizer and static-analysis gates pass;
- teardown completes without forced account deletion or busy-state cleanup failures;
- architecture, usage, interview, and evidence documentation are present;
- committed evidence contains no SIP credentials or sensitive raw traces;
- milestone tag `v0.6-pjsip-cli` identifies the completed phase.

## References

- `docs/architecture/pjsip-cli.md` — Architecture
- `docs/phase5/dependencies.md` — Dependency provisioning
- `docs/interview/phase5-questions.md` — Interview review
- `evidence/phase5/` — Evidence

## Next phase

Phase 6 adds the real RTP/audio path: capture/playback devices, PJSUA2 media connections, codec/media verification, and diagnosis of one-way or missing audio.
