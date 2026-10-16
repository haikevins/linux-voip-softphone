# Phase 8 Evidence — Multithreading

## Status

Completed. Phase 8 established the multithreading/concurrency architecture and the `v0.9-threading` milestone.

## Verification scope

Evidence covers the Phase 8 worker-execution model, threaded SIP backend ownership/lifecycle, reusable thread-safe queue behavior, 100,000-event multi-producer/multi-consumer stress verification, full regression, static analysis, ThreadSanitizer build coverage, and the required CI runtime ThreadSanitizer gate.

## Environment

The accepted Phase 8 implementation/documentation revision before closure is `38217be`. Local verification used GCC 13.3.0, GoogleTest 1.14.0, the existing Qt/PJPROJECT development environment, and the repository CMake presets. The local host can build TSan-instrumented binaries but does not reliably support the ThreadSanitizer runtime because of the previously observed shadow-memory mapping limitation.

## Automated verification

Run:

```bash
export PKG_CONFIG_PATH="$HOME/.local/pjproject-2.17/lib/pkgconfig${PKG_CONFIG_PATH:+:$PKG_CONFIG_PATH}"
./scripts/phase8/verify.sh
```

The accepted local verifier reported 17/17 Phase 8 tests passing and 94/94 total non-PJSIP tests passing, then completed a fresh ThreadSanitizer build. The Phase 8 branch CI completed 4/4 required jobs successfully at revision `38217be`. See `automated-verification.txt`.

## Manual verification

No separate human-operated SIP/audio acceptance is required by the Phase 8 concurrency milestone. The live GUI/SIP/audio behavior remains covered by the completed Phase 7 evidence; Phase 8 acceptance focuses on execution context, queue/lifecycle behavior, stress verification, regression, and dynamic race detection.

## Artifacts

- `automated-verification.txt` — concise local/CI Phase 8 gate summary.
- `stress-test.txt` — accepted 100,000-event queue stress-test configuration and result.
- `tsan-report.txt` — local TSan build result, local runtime limitation, and CI runtime result.

These are curated summaries rather than full terminal transcripts.

## Results

The Phase 8 automated verifier passed, including 17 Phase 8 tests and the 94-test full regression. The 100,000-event queue stress test passed with four producers and four consumers, and the branch CI completed all four required jobs including the ThreadSanitizer runtime gate. The result supports the documented threading/lifecycle design for the tested revision and environments; it does not prove the absence of every possible concurrency defect.

## Security and redaction

The retained summaries contain no SIP password, Digest Authorization material, SRTP key material, raw SIP/PJSIP authentication trace, or packet capture. Phase 8 evidence records only concurrency/build/test outcomes needed for the milestone.

## Reproduction

Run the complete Phase 8 verifier:

```bash
export PKG_CONFIG_PATH="$HOME/.local/pjproject-2.17/lib/pkgconfig${PKG_CONFIG_PATH:+:$PKG_CONFIG_PATH}"
./scripts/phase8/verify.sh
```

On a host that supports ThreadSanitizer runtime execution, add:

```bash
RUN_TSAN_TESTS=1 ./scripts/phase8/verify.sh
```

On the documented local host, leave `RUN_TSAN_TESTS` unset and use the required CI ThreadSanitizer job for runtime race detection.

## References

- Phase guide: `docs/phase8/README.md`
- Architecture: `docs/architecture/threading-model.md`
- Interview review: `docs/interview/phase8-questions.md`
- Automated verifier: `scripts/phase8/verify.sh`
- Worker tests: `tests/unit/test_worker_executor.cpp`
- Threaded SIP tests: `tests/unit/test_threaded_sip_engine.cpp`
- Queue tests: `tests/unit/test_thread_safe_queue.cpp`
- Stress test: `tests/stress/thread_safe_queue_stress.cpp`
