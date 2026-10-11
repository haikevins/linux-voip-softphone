# Documentation Taxonomy and Editorial Standard

The project uses **five separate Markdown document families**, each with one responsibility, one heading schema, and one writing style. Documents within a family should read as parts of the same series. Documents in different families must not be forced into a single shared outline.

| Family | Authoritative question | Primary location |
|---|---|---|
| 1. Architecture | How is the system designed, owned, and operated? | `docs/architecture/*.md` |
| 2. Interview | Can the engineering decisions and limits be explained? | `docs/interview/phaseN-questions.md` |
| 3. Phase | What was delivered, how is it verified, and is the milestone accepted? | `docs/phaseN/README.md` |
| 4. Protocols | What do the protocol messages/packets mean on the wire? | `docs/protocols/*.md` |
| 5. Evidence | What was actually observed and retained? | `evidence/phaseN/README.md` |

`docs/phaseN/*.md` may contain focused phase-specific procedures, such as a lab setup or a dependency guide. They are supporting material, **not** a sixth document family and not substitutes for a phase README. `docs/README.md` and `evidence/README.md` govern the taxonomy; top-level project/roadmap READMEs and scripts have their own responsibilities.

## Shared editorial rules

**Language and formatting**

- Keep the existing project documentation in technical English. Use one clear term for a concept throughout a family; do not alternate between Vietnamese and English descriptions or invent synonyms for status labels.
- Each file has one informative `#` title, the family's exact `##` section sequence, and optional `###` subsections. Interview question headings use `####` under a `###` topic.
- Start with a direct statement of fact or purpose. Prefer short, declarative sentences and specific nouns/identifiers over promotional adjectives or broad guarantees.
- Use present tense for the current design and reproducible procedures; past tense for recorded experiments and historical milestones; future tense only for explicitly planned work.
- Quote paths, identifiers, commands, tags, API names, and literal SIP/RTP fields with backticks. Keep reference bullets path-first (`- \`path\``), with an optional `— explanation` after the path.
- Use fenced `bash` for runnable shell commands and `text` for topology, state, or message-flow diagrams. Do not imply an example command was executed unless evidence records it.
- Do not add Markdown completion checkboxes; distinguish completion, observations, blockers, and untested properties in prose or a labeled table.

**Authority and traceability**

- Architecture owns **design and lifetime contracts**; protocols own **wire semantics**; phase READMEs own **milestone and reproduction entry points**; interview pages own **explanations**; evidence owns **observations and historical results**.
- State a technical fact fully in its authoritative home; in the other families, provide a concise relevant summary and a path to that source. A reference is not a reason to copy large sections verbatim.
- A passing automated gate supports only the checks actually executed. A manual lab observation is not a general product guarantee. An `UNPROVEN` property must never silently become `PASS` through wording changes.
- Preserve historical facts (environment, result counts, revision, dates, and failed or superseded observations). If a later milestone supersedes a claim, label the older statement as historical and link the accepted result.
- Never commit passwords, SIP Digest response/nonces, secret-bearing transcripts, SRTP inline keys, raw protected packet captures, audio payloads, or unrelated generated files.

**Status vocabulary**

| Label | Meaning | Editorial use |
|---|---|---|
| `Completed` | A phase milestone has a recorded completed release/tag | Phase and historical evidence status; always name the actual tag |
| `PASS` | A defined check succeeded in its recorded scope | Individual test/gate results only; give a command or retained artifact |
| `OBSERVED/EVIDENCED` | A controlled observation was captured | Manual/live findings; describe its environment and limitations |
| `UNPROVEN` | A property has not been sufficiently demonstrated | Keep separate from passing related tests |
| `BLOCKED` | A required integration/readiness decision cannot be approved | Name the unresolved dependency |
| `PENDING` | Formal acceptance has not been recorded | Do not equate merging to `main` with acceptance |

Phase 8 is **merged to `main` but pending final maintainer acceptance**. Offline checks and a controlled PJSUA2 `PJ_EBUSY` observation do not prove live external callback quiescence or production-client real `PJ_EBUSY` recovery; PJSUA2 integration readiness remains `BLOCKED`. These qualifiers must agree in `docs/phase8/README.md`, `docs/architecture/threading-model.md`, `docs/interview/phase8-questions.md`, and `evidence/phase8/README.md`.

## 1. Architecture — `docs/architecture/`

**Responsibility:** describe implemented components, dependencies, ownership, thread boundaries, states/data flows, lifecycle, trade-offs, verification *strategy*, and what the architecture does not prove. Do not place the operator's lab transcript, milestone checklist, or a protocol primer here.

The top-level headings must appear in this order:

```text
# Phase N Architecture — <Topic>

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

| Section | Required content |
|---|---|
| Status | Actual milestone/acceptance state; distinguish a completed phase from open Phase 8 risks |
| Purpose | The design problem solved and why this component exists |
| Scope | Included components and explicit non-goals, without claiming later-phase features |
| Architecture model | Named source components, ownership edges, dependency directions, diagrams as needed |
| Runtime and lifecycle | Startup, steady state, callbacks, failure/timeout paths, and teardown in actual order |
| Verification strategy | Which tests or gates exercise the design and their coverage limits; link actual runs to evidence |
| Design rationale | Why key alternatives/trade-offs were accepted or rejected |
| Phase boundary | What this phase establishes and what must be handled elsewhere |
| References | Phase README, relevant source/tests, interview/evidence or protocol documents as needed |

Write contracts in present tense. Mention historical runs briefly only when necessary to explain the boundary; the authoritative numbers and observations belong in `evidence/`.

## 2. Interview — `docs/interview/`

**Responsibility:** turn project-specific engineering decisions into questions with technically supported answers. This is a review guide, not a glossary, runnable verification procedure, or evidence record.

```text
# Phase N Interview Review — <Topic>

## Review scope
## Questions and answers
### <Topic area>
#### <Full engineering question?>
<Direct answer, relevant mechanism, trade-off, and limitation.>
## Phase boundary
## References
```

Keep the same opening instruction under **Review scope** across phases. Group questions under `###` topic headings; each `####` is a complete question ending with `?`, followed by a concrete answer. Tie explanations to repository classes, actual call paths, tests, or observed outcomes. Say what the mechanism **does not** guarantee. Do not reintroduce phase-wide status tables or per-commit closure logs into interview pages.

## 3. Phase README — `docs/phaseN/README.md`

**Responsibility:** be the single entry point for phase delivery, repeatable commands, acceptance scope, and navigation. It summarizes architecture, protocols, interview topics, and evidence rather than replacing them.

```text
# Phase N — <Topic>

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

| Section | Required content |
|---|---|
| Status | `Completed` with actual milestone tag or explicit `PENDING`; state narrower lab acceptance when needed |
| Objective | One concrete outcome for the phase |
| Scope | Included work **and** excluded work, distinguished from later milestones |
| Deliverables | Actual implemented artifacts/paths, not a wish list |
| Architecture and design | Brief implementation sketch with architecture link |
| Build and run | Commands starting from repository root; declare optional toolchain/dependency assumptions |
| Verification | How to rerun automated checks; keep manual/live steps separate and qualify historical counts |
| Evidence | Link to `evidence/phaseN/README.md`; state redaction boundary |
| Interview focus | Skills/questions to explain, linked to `docs/interview/` when present |
| Exit criteria | Measurable accepted vs. blocked/unproven criteria; never equate passing subset with full release |
| References | Canonical sources and useful implementation/test paths |
| Next phase | Transition rule; Phase 8 must not launch Phase 9 by inference |

Phase 0 may legitimately have no matching interview Q&A document. Phase-specific procedure pages stay adjacent to the README and are referenced, not copied wholesale.

## 4. Protocols — `docs/protocols/`

**Responsibility:** explain normative SIP/RTP meaning and precisely scoped on-wire observations. Distinguish protocol behavior from this application's ownership decisions and one local capture.

```text
# <Protocol> Protocol — <Topic>

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

Use RFC terminology and relevant message/field names. Diagrams/tables should separate requests, responses, media directions, and timing. Mark **general protocol properties** separately from **single-capture or lab observations**, including missing capture legs and possible alternative causes. Keep lab installation steps under phase documentation, PJSUA2 ownership under architecture, and raw/retained observations under evidence. An RFC may appear as a plain bibliographic reference; project paths remain backticked.

## 5. Evidence — `evidence/phaseN/README.md`

**Responsibility:** index sanitized historical results, not design intentions. Evidence pages answer **what ran, where, what was observed, where the retained artifact is, and how a new run can be reproduced**.

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

| Section | Required content |
|---|---|
| Status | State of the recorded milestone; do not turn a pending acceptance into a completion |
| Verification scope | Tested scenarios and explicit limits |
| Environment | Historical OS/toolchain/lab assumptions, or a direct reference to the retained environment record |
| Automated verification | Executed/reproducible gate, observed results and counts where evidenced |
| Manual verification | Real human/live lab observations, or a clear **not applicable/not performed** statement |
| Artifacts | Each retained filename (or scenario index) plus its specific evidentiary purpose |
| Results | A concise result qualified by the exact scope; preserve `UNPROVEN`/`BLOCKED` statements |
| Security and redaction | What sensitive source material was excluded |
| Reproduction | Current commands and prerequisites; explicitly separate a new run from historical evidence |
| References | Matching phase README and relevant design/test entry points |

Evidence scenario pages (currently Phase 4 registration and call flow) use their separate eight-heading scenario schema defined in `evidence/README.md`; they are not phase evidence indexes. Missing evidence must be described as missing, not invented for symmetry. Preserve old observations unless a deliberately new, clearly attributed run supersedes them.

## New-phase documentation contract

For a new Phase N, create/update material in this order:

1. Establish scope, explicit non-goals, deliverables, and acceptance criteria in `docs/phaseN/README.md`.
2. Add or update only the architecture and protocol pages whose **actual** design/wire semantics changed. Cross-link rather than duplicate older explanations.
3. Add `docs/interview/phaseN-questions.md` for the decisions and verification limits that are worth explaining.
4. Create `evidence/phaseN/README.md` before collecting artifacts. Keep automated and manual results separate; add only sanitized artifacts that support a claim.
5. Reconcile phase status and boundary claims across all five families and `ROADMAP.md`. Only advance the release status after an explicit acceptance decision.
6. Verify the Markdown schemas, references, and evidence secrecy checks before committing. Preserve historical measurements and run results.

## Structural and editorial verification

From repository root:

```bash
bash scripts/quality/verify-docs-structure.sh
bash scripts/quality/verify-evidence-structure.sh
python3 scripts/quality/verify-documentation-style.py
```

The existing shell gates check the two taxonomies and evidence safety rules. The additional standard-library checker validates all five family schemas, matching phase numbers, nonempty required sections, readable interview questions, phase/evidence pairs, and reference targets. Automated checks cannot determine whether a technical claim is true; review it against code, actual runs, and the project roadmap.
