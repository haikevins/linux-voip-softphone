# Phase 8 Evidence — Multithreading

## Status

Phase 8 evidence is **closure-candidate evidence, not final release sign-off**.
The recorded ThinkPad offline/runtime gates include successful canonical,
focused, stress, TSan, PJSIP non-live, and GUI-preview acceptance results.

Final maintainer Phase 8 sign-off remains pending. Production PJSUA2 shutdown
integration remains `BLOCKED/UNPROVEN` because live external callback
quiescence and production-client race coverage are not established. A later
controlled PJSUA2 2.17 lab run records a real `PJ_EBUSY` recovery sequence
without upgrading that broader integration boundary.

The roadmap-required closure artifacts are retained separately in
`evidence/07-threading/`; this directory preserves the chronological Phase 8
verification record.

## Verification scope

Evidence in this directory covers:

- queue/worker behavior and ownership;
- canonical CTest integration;
- 100,000-event MPMC stress and repeated stress runs;
- standalone ThreadSanitizer;
- Qt/PJSUA2 ownership/source review;
- PJSUA2 call-slot cleanup policy;
- callback activity and production-lifetime audits;
- shutdown failure-path, callback-ingress, ownership-policy, and
  authorization/integration-contract models;
- CI formatting/tidy corrections;
- manual-verification status for slices that did not alter live SIP behavior.

It does not prove live PJSUA2 callback quiescence, safe teardown after every
shutdown failure, production-client recovery from every real `PJ_EBUSY`
timing, universal race freedom, memory leak freedom, or sustained GUI FPS
performance. The controlled live evidence does prove that one real PJSUA2 2.17
non-forced shutdown returned `PJ_EBUSY` with an active call slot and that the
recorded retry succeeded after hangup and zero call slots.

## Environment

Primary recorded environment is the maintainer ThinkPad/Linux development
machine used by this repository. Relevant recorded tool/runtime observations
include:

- C++20 / CMake / Ninja / CTest;
- Qt 6 GUI build where required;
- PJSIP/PJSUA2 2.17 for canonical non-live regression where required;
- Clang 18.1.3 for successful standalone TSan verification;
- GCC 13 TSan on the ThinkPad recorded as BLOCKED because of sanitizer
  runtime memory-mapping failures rather than reported as clean.

Individual artifacts identify their own exact baseline/tool invocation where
that detail is relevant.

## Automated verification

The chronological automated evidence files are:

- `automated-verification.txt` — queue/worker/canonical baseline history;
- `stress-test.txt` — MPMC stress and repeat evidence;
- `tsan-report.txt` — standalone TSan results and blocked-toolchain notes;
- `ownership-audit.txt` and `ownership-source-review.txt` — Qt/PJSUA2 ownership
  verification boundaries;
- `pjsip-shutdown-order.txt` — bounded call cleanup ordering;
- `pjsip-cleanup-timeout.txt` — deterministic call-slot polling policy;
- `callback-activity.txt` — callback activity/drain model;
- `pjsip-production-lifetime-audit.txt` — production lifetime source audit;
- `ci-clang-format-hotfix.txt` and `clang-tidy-regression-hotfix.txt` — quality
  gate corrections;
- `pjsip-shutdown-failure-paths.txt` — failure-path model;
- `pjsip-callback-ingress-races.txt` — external callback ingress races;
- `pjsip-shutdown-ownership-policy.txt` — retry/stale-result ownership model;
- `pjsip-shutdown-authorization.txt` — authorization policy;
- `pjsip-shutdown-integration-contract.txt` — fail-closed integration gate;
- `pjsip-live-lifecycle.txt` — sanitized controlled PJSUA2 2.17 live lifecycle
  observation, including real `PJ_EBUSY` and successful retry after hangup.

Roadmap closure and final offline runtime evidence are indexed from
`evidence/07-threading/`. The final post-hardening rerun is also preserved as
`final-runtime-acceptance-8c78677.txt` so the exact committed HEAD and observed
manual metrics remain explicit without rewriting earlier chronological files.

## Manual verification

`manual-verification.txt` preserves the historical manual status for the early
isolated concurrency slice. Those commits changed no SIP/GUI runtime behavior,
so live call testing was not recorded as if it had occurred.

The later final offline GUI acceptance is recorded in:

```text
evidence/07-threading/final-runtime-acceptance.txt
```

That record includes human preview-mode GUI interaction and normal window
X-close PASS. It explicitly states that live SIP registration/calls were not
tested by that gate. The later `pjsip-live-lifecycle.txt` artifact is a
separate controlled lab observation and does not rewrite the historical GUI
acceptance scope.

## Artifacts

Historical Phase 8 artifacts are the files listed in this directory. The latest
sanitized final rerun summary is `final-runtime-acceptance-8c78677.txt`.
Roadmap-required final artifacts are:

```text
evidence/07-threading/
├── README.md
├── stress-test.txt
├── tsan-report.txt
├── threading-diagram.png
└── final-runtime-acceptance.txt
```

The diagram is descriptive code-level architecture derived from
`docs/architecture/threading-diagram.dot`; it is not a runtime thread trace.

## Results

Recorded final offline acceptance rerun on committed HEAD `8c78677` reports:

- documentation structure: PASS;
- evidence structure: PASS;
- source checkout cleanliness: PASS;
- canonical CTest: 147/147 PASS;
- Qt worker repeat: 10/10 PASS;
- focused Phase 8: 12/12 PASS;
- MPMC 100,000-event stress: 30/30 PASS in the final run, with earlier 100/100
  repeated-stress evidence retained;
- Clang 18 TSan: 18/18 PASS;
- PJSIP non-live regression: 45/45 PASS;
- GUI headless startup/exit: 40/40 PASS (median 192.75 ms, P95 202.69 ms,
  maximum 214.44 ms);
- human preview GUI interaction: PASS over 111.47 seconds;
- normal X-close: PASS, exit code 0;
- manual observation: 222 samples, RSS 193376/198280/199628 KiB
  first/last/peak, FD range 30–33, thread range 23–24.

Results must be read with their scope. The controlled live PJSUA2 2.17 run
observed real `PJ_EBUSY`, zero call slots after hangup, successful non-forced
shutdown retry, and no late registration callback (`onRegState`) in its recorded observation
window. Integration readiness remains BLOCKED; external callback quiescence
and production-client recovery across adversarial timings remain UNPROVEN.

## Security and redaction

Do not commit:

- SIP account passwords;
- raw `Authorization: Digest ...` material;
- nonce/response material that can expose live authentication state;
- SRTP `a=crypto` inline key material;
- raw PCAP/PCAPNG captures containing sensitive traffic;
- unrelated build trees or core dumps.

Phase 8 automated/offline gates do not require SIP credentials. If future live
SIP evidence is collected, retain only the sanitized facts required to support
the result.

## Reproduction

Core Phase 8 verification entry points include:

```bash
bash scripts/phase8/verify-queue.sh
PHASE8_STRESS_REPEATS=100 bash scripts/phase8/verify-stress.sh
CXX="$(command -v clang++-18)" \
PHASE8_TSAN_BUILD_DIR="$PWD/build/phase8-tsan-clang18-debug" \
PHASE8_TSAN_REPEATS=3 \
    bash scripts/phase8/verify-tsan.sh
bash scripts/phase8/verify-pjsip-live-lifecycle-evidence.sh
bash scripts/phase8/verify-roadmap-exit.sh
bash scripts/phase8/verify-final-runtime-acceptance.sh --automatic
```

The human stage for the final runtime acceptance requires the actual
`EVIDENCE_DIR` emitted by the automatic run and a graphical desktop terminal.
See `docs/phase8/final-runtime-acceptance.md` before running it. The live
lifecycle experiment is intentionally separate and requires explicit opt-in:

```bash
PHASE8_LIVE_SIP=1 bash scripts/phase8/run-pjsip-live-lifecycle-probe.sh
```

A reproduced run creates new observations. Do not overwrite old evidence to
make historical runs appear current.

## References

- `docs/phase8/README.md`
- `docs/architecture/threading-model.md`
- `docs/phase8/roadmap-exit-review.md`
- `docs/phase8/final-runtime-acceptance.md`
- `docs/interview/phase8-questions.md`
- `README_roadmap.md`, Phase 8 — Multithreading
- `evidence/07-threading/README.md`
- `tests/phase8/`
- `scripts/phase8/`
