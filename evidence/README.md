# Evidence

`evidence/` stores sanitized verification outcomes for project milestones. It is not a second documentation tree and it does not contain the executable verification logic.

```text
docs/      explanation and design
tests/     executable verification logic
scripts/   verification/collection orchestration
evidence/  retained observed results
```

## Evidence responsibilities

A phase evidence set should make it possible to answer:

- what was verified;
- under which environment or assumptions;
- which automated/manual checks ran;
- what result was observed;
- which retained artifact supports the result;
- how to reproduce the check.

Architecture, protocol tutorials, implementation guidance, and interview material belong under `docs/`.

## Layout

Evidence is organized by milestone:

```text
evidence/
├── README.md
├── phase0/
├── phase1/
├── phase2/
├── phase3/
├── phase4/
├── phase5/
├── phase6/
├── phase7/
└── phase8/
```

A phase may contain additional scenario directories or artifacts when they represent a real verification boundary. Symmetry between phases is not required.

Commit-by-commit/checkpoint history belongs in Git history. The current evidence tree keeps only the concise artifacts needed to support milestone claims and reproducible verification.

## Phase README schema

Each `evidence/phaseN/README.md` uses:

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

Scenario-specific READMEs use:

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

## Retention policy

Keep an artifact when it provides useful historical proof that cannot be replaced by a short summary alone. Prefer small, deterministic, sanitized artifacts over complete terminal transcripts.

Do not retain per-commit progress logs when Git history already preserves them. New evidence should avoid large logs when a concise summary plus a reproducible verification command is sufficient.

## Security and redaction

Never commit:

```text
SIP account passwords
raw secret-bearing SIP traces
live Digest Authorization response material
SRTP inline key material
*.pcap / *.pcapng containing sensitive traffic or audio payload
core dumps
unrelated generated build trees
```

Record only the sanitized outcome needed to support the verification claim.

## Verification

Run:

```bash
./scripts/quality/verify-evidence-structure.sh
```

The verifier checks evidence taxonomy, README structure, forbidden capture extensions, common secret-bearing patterns, legacy paths, and Markdown checkbox syntax.

For a new phase, create the phase README first, keep automated and manual results distinct, add only necessary artifacts/scenario directories, and run the verifier before committing.
