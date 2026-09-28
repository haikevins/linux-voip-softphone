# Documentation Taxonomy and Structure

This directory separates documentation by responsibility so the same subject is not repeatedly explained from architecture, protocol, phase-execution, interview, and evidence perspectives in one file.

## Documentation namespaces

| Namespace | Primary question it answers | Content boundary |
|---|---|---|
| `docs/architecture/` | How is the software/system structured and why? | Components, ownership, dependencies, lifecycle, data/control flow, design rationale |
| `docs/protocols/` | How does the protocol behave on the wire/session boundary? | Roles, messages/packets, state, interactions, failure modes, security observations |
| `docs/interview/` | What should be explainable in a technical review? | Questions, answers, trade-offs, phase boundary |
| `docs/phaseN/README.md` | What did this phase build, how is it run, and how is it verified? | Objective, scope, deliverables, build/run, verification, evidence, references |
| `docs/phaseN/*.md` | What phase-specific operational detail is too large for the phase README? | Focused lab/setup/dependency procedures only |
| `evidence/` | What concrete result demonstrates that a verification step ran? | Sanitized outputs and observed state, not explanatory design documentation |

A document should have one primary responsibility. Cross-namespace duplication should be replaced by a short summary plus a reference to the authoritative document.

## Architecture document schema

Every Markdown file directly under `docs/architecture/` uses these H2 headings in this exact order:

```text
## Status
## Purpose
## Scope
## Architecture model
## Runtime and lifecycle
## Verification strategy
## Design rationale
## Phase boundary
## References
```

Architecture documents may use H3/H4 headings inside those sections when the subject requires more detail.

## Protocol document schema

Every Markdown file directly under `docs/protocols/` uses these H2 headings in this exact order:

```text
## Status
## Purpose
## Scope
## Roles and terminology
## Wire model
## Runtime behavior
## Interactions and dependencies
## Failure modes
## Security considerations
## Verification observations
## Phase boundary
## References
```

This schema is intentionally protocol-neutral so it can be reused for SIP, RTP, RTSP, and later protocol notes. A protocol may state that a section is not applicable rather than inventing behavior that belongs to another layer.

## Interview document schema

Every Markdown file directly under `docs/interview/` uses these H2 headings in this exact order:

```text
## Review scope
## Questions and answers
## Phase boundary
## References
```

Topic-specific question groups use H3 headings under `Questions and answers`; individual questions use H4 headings.

The filename convention is:

```text
phaseN-questions.md
```

The H1 convention is:

```text
# Phase N Interview Review — <Phase topic>
```

## Phase README schema

Every primary `docs/phaseN/README.md` uses these H2 headings in this exact order:

```text
## Status
## Objective
## Scope
## Deliverables
## Architecture and design
## Build and run
## Verification
## Evidence
## Interview focus
## Exit criteria
## References
## Next phase
```

The phase README is the entry point, not a second architecture or protocol reference. It should summarize those subjects and link to the authoritative namespace when deeper explanation exists.

## Phase-specific supplemental documents

Supplemental files under `docs/phaseN/` are allowed only when they describe phase-specific execution details that would make the primary README unwieldy. Examples include a lab procedure or dependency-provisioning guide.

They do not need a global schema because their responsibilities differ, but they must satisfy all of these rules:

- the phase README links to them;
- they remain phase-specific rather than becoming general architecture/protocol references;
- they do not duplicate evidence artifacts;
- they do not contain credentials or raw authentication-bearing traces;
- when their content becomes cross-phase knowledge, it moves to the appropriate top-level namespace.

## Linking and evidence rules

Architecture documents may reference protocol documents for wire semantics. Protocol documents may reference architecture documents for deployment/component context. Phase READMEs summarize both and point to the authoritative files. Interview documents reference the implementation/docs needed to support each answer.

Evidence is intentionally separate from explanatory documentation. Raw SIP traces, credentials, Digest authorization material, and media keying material are not committed.

## Structural verification

Run:

```bash
./scripts/verify_docs_structure.sh
```

The verifier checks the required H1/H2 conventions for architecture, protocol, interview, and phase README documents and rejects Markdown checkbox syntax that would reintroduce inconsistent completion-state formatting.
