# Evidence

## Purpose

`evidence/` records verification outcomes that support completed project phases. It is not a second documentation tree and it is not the implementation of the tests themselves.

The repository separates responsibilities as follows:

```text
docs/      explanation, design, protocol knowledge, and phase guidance
tests/     executable verification logic
scripts/   verification and collection orchestration
evidence/  recorded verification outcomes and retained artifacts
```

## Responsibility boundaries

Evidence answers questions such as:

- What was verified?
- Under which environment or runtime assumptions?
- Which automated or manual checks were performed?
- What result was observed?
- Which retained artifact supports that result?
- How can the verification be reproduced?

Evidence must not become the authoritative home for architecture, protocol tutorials, interview review, or phase implementation guidance. Those belong under `docs/`.

## Directory layout

Phase evidence uses one stable naming scheme:

```text
evidence/
├── README.md
├── phase0/
│   └── README.md
├── phase1/
│   └── README.md
├── phase2/
│   └── README.md
├── phase3/
│   └── README.md
├── phase4/
│   ├── README.md
│   ├── registration/
│   ├── call-flow/
│   └── artifacts/
├── phase5/
│   └── README.md
├── phase6/
│   ├── README.md
│   ├── automated-verification.txt
│   ├── live-registration.txt
│   ├── live-outgoing-call.txt
│   ├── live-incoming-call.txt
│   └── phase6-summary.txt
└── phase7/
    ├── README.md
    ├── automated-verification.txt
    ├── manual-verification.txt
    ├── runtime-media.txt
    ├── login.png
    ├── dialer.png
    ├── incoming-call.png
    ├── active-call.png
    └── diagnostics.png
```

A phase may contain additional artifacts or scenario directories only when they represent real verification needs. Directory symmetry is not required.

## Phase README schema

Every `evidence/phaseN/README.md` uses these H2 sections in this exact order:

```text
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

The phase README is the evidence index and summary. It should stay concise and should not duplicate full command logs.

## Scenario README schema

When a phase needs scenario-specific evidence, each scenario README uses:

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

Phase 4 currently uses this schema for SIP registration and call-flow evidence.

## Artifact retention policy

Retain an artifact when it provides useful proof that is difficult to reconstruct from a short summary. Prefer small, deterministic, sanitized artifacts over complete terminal transcripts.

Historical artifacts already committed by completed phases are preserved unless they are duplicate, generated without evidentiary value, or sensitive. New evidence should avoid collecting large logs by default when a concise summary and a reproducible verification command are sufficient.

`README.md` files summarize outcomes; they do not replace the underlying evidence when a retained artifact is needed.

## Security and redaction

Evidence must never contain SIP account passwords or secret-bearing authentication material. Raw SIP logging and packet captures must not be committed when they can expose Digest authorization material, SRTP key material, credentials, or other secrets.

Do not commit:

```text
*.pcap
*.pcapng
raw secret-bearing SIP traces
Digest Authorization headers with live response material
SRTP inline key material
core dumps
unrelated generated build trees
```

When a manual SIP verification is recorded, capture only sanitized outcomes needed to prove the behavior.

## Verification contract

Run:

```bash
./scripts/quality/verify-evidence-structure.sh
```

The verifier checks directory naming, README schema, legacy evidence paths, forbidden capture extensions, common secret-bearing SIP patterns, and Markdown checkbox syntax within the evidence tree.

## Adding new phase evidence

For a new phase:

1. Create `evidence/phaseN/README.md` using the common phase evidence schema.
2. Record automated and manual verification separately.
3. Add scenario directories only for real verification boundaries.
4. Keep knowledge and design explanations in `docs/`.
5. Retain only artifacts that materially support a result.
6. Run the evidence structure verifier before committing.
