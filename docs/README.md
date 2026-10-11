# Documentation Taxonomy and Structure

`docs/` separates explanatory material by responsibility. A subject should have one authoritative home; other documents summarize it and link to that source instead of repeating it.

## Documentation namespaces

| Namespace | Responsibility |
|---|---|
| `docs/architecture/` | Components, ownership, lifecycle, dependencies, data/control flow, and design rationale |
| `docs/protocols/` | Wire/session behavior, roles, messages/packets, failure modes, and security observations |
| `docs/interview/` | Phase review questions, engineering trade-offs, and explainable conclusions |
| `docs/phaseN/README.md` | Phase objective, scope, deliverables, build/run, verification, evidence, and boundary |
| `docs/phaseN/*.md` | Focused phase-specific procedures that would make the phase README unwieldy |
| `evidence/` | Sanitized observed results and retained artifacts, not design documentation |

## Required schemas

Architecture documents directly under `docs/architecture/` use:

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

Protocol documents directly under `docs/protocols/` use:

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

Interview documents use:

```text
# Phase N Interview Review — <Phase topic>

## Review scope
## Questions and answers
## Phase boundary
## References
```

Primary phase READMEs use:

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

## Content rules

- Keep architecture, protocol, phase execution, interview review, and evidence separate.
- Use a short summary plus a reference when deeper explanation already exists elsewhere.
- Phase-specific supplemental documents must remain phase-specific and be referenced by the phase README.
- Move cross-phase design knowledge to `docs/architecture/` or `docs/protocols/`.
- Do not duplicate evidence artifacts inside explanatory documentation.
- Do not commit credentials, authentication-bearing raw SIP traces, media keying material, or other secrets.

## Structural verification

Run:

```bash
./scripts/quality/verify-docs-structure.sh
```

The verifier checks required H1/H2 conventions and rejects Markdown checkbox syntax that would reintroduce inconsistent completion-state formatting.
