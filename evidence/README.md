# Evidence Taxonomy and Retention Standard

`evidence/` is the project's source of truth for **sanitized recorded verification outcomes**. It is not a second architecture or phase documentation tree, and it does not contain executable test/verification logic. The five separate documentation families and their writing rules are defined in `docs/README.md`.

```text
docs/architecture/       design and ownership contracts
docs/interview/          project-specific engineering explanations
docs/phaseN/README.md    phase delivery, commands, acceptance criteria
docs/protocols/          SIP/RTP wire and session semantics
evidence/phaseN/README.md retained observations and artifact index

tests/                   executable assertions
scripts/                 verification and evidence-collection orchestration
```

## Evidence responsibilities

A verification claim must be traceable to **scope → environment → method → observation → retained artifact (where applicable) → limitation**.

- **Recorded**: describe what was actually run and observed, with historical counts/dates only when supported by a retained record.
- **Reproducible**: give a current command or a lab procedure; explain external prerequisites. A new run is not automatically the same as the recorded result.
- **Bounded**: a passing unit test, manual call, or single packet capture establishes no more than its stated scope.
- **Separated**: automated tests and manual/operator observations go in distinct sections; do not silently convert one into the other.
- **Sanitized**: retain only necessary proof and do not copy raw credentials, protected SIP media, or complete terminal transcripts into a README.

For milestones with a tag, record the actual tag in **Status**. For Phase 8 use `PENDING` for maintainer acceptance, `OBSERVED/EVIDENCED` for controlled `PJ_EBUSY` recovery, `UNPROVEN` for live callback quiescence and production-client real recovery, and `BLOCKED` for PJSUA2 integration readiness. Never label all Phase 8 integration readiness as passed because offline checks passed.

## Layout

```text
evidence/
├── README.md
├── phase0/
├── phase1/
├── phase2/
├── phase3/
├── phase4/
│   ├── registration/
│   └── call-flow/
├── phase5/
├── phase6/
├── phase7/
└── phase8/
```

Every `docs/phaseN/README.md` requires a matching `evidence/phaseN/README.md`. Scenario subdirectories exist only when they represent genuinely distinct verification boundaries; do not create them solely to make two phases look symmetrical. Milestone and per-commit history live in Git; evidence retains the small, necessary outcomes not reconstructed by Git history alone.

## Phase README schema

Every `evidence/phaseN/README.md` has **exactly** the following second-level headings, in order:

```text
# Phase N Evidence — <Topic>

## Status
## Verification scope
## Environment
## Automated verification
## Manual verification
## Artifacts
## Results
## Security and redaction
## Reproduction
## References
```

Within **Artifacts**, use path-first bullets with an explanatory purpose, for example `- \`stress-test.txt\` — repeated MPMC stress observation.`. Point to the real artifact; do not invent a filename, a test count, or a successful result. **Manual verification** must explicitly state when no manual action is applicable or when a required live test has not been performed.

The standalone verifier at `scripts/quality/verify-documentation-style.py` checks that every phase evidence index exists, follows this schema, and references valid paths. The evidence safety verifier continues to enforce the permitted tree and redaction rules.

## Scenario README schema

The Phase 4 scenario READMEs (`registration/README.md` and `call-flow/README.md`) retain a separate schema:

```text
## Purpose
## Preconditions
## Procedure
## Observations
## Result
## Artifacts
## Security and redaction
## References
```

A scenario README explains an **actual verification scenario**, not the whole phase. The phase index summarizes its outcome and links to it. Preserve earlier unsuccessful observations if they explain a later correction; mark superseded outcomes clearly and describe the accepted result separately.

## Retention policy

Retain only evidence that supports a useful, historically bounded engineering claim. Prefer concise, deterministic and sanitized summaries; reference an existing small artifact rather than copying its contents into multiple documents.

Never rewrite historical logs or result counts solely to make the tree look current. When a new test run is performed, retain it as a new, attributable observation if there is a reason to preserve it. If an expected artifact is absent, state that fact; do not fabricate an empty placeholder.

Keep design explanations under `docs/architecture/`, protocol exposition under `docs/protocols/`, milestone execution under `docs/phaseN/`, and Q&A under `docs/interview/`.

## Security and redaction

Do **not** commit any of these:

```text
SIP account passwords
raw Digest Authorization response/nonce/cnonce material
raw secret-bearing SIP/PJSIP traces
SRTP inline key material
*.pcap / *.pcapng with protected network/audio payloads
raw audio, core dumps, or unrelated build trees
```

A redacted summary should include only the necessary environment details, signaling/call outcome, bounded RTP/call statistics, and limitations. Sanitizing a log does not by itself authorize retaining unrelated peer identifiers.

## Verification and new phases

From the repository root:

```bash
bash scripts/quality/verify-docs-structure.sh
bash scripts/quality/verify-evidence-structure.sh
python3 scripts/quality/verify-documentation-style.py
```

For each new phase, first create the matching phase and evidence indexes, write verification *intent* in `docs/phaseN/README.md`, then record *observed outcomes* under `evidence/phaseN/README.md` after running the checks. Separate automated and manual evidence, keep only necessary sanitized artifacts, and reconcile status with the roadmap before final acceptance.
