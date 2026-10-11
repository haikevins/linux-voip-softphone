#!/usr/bin/env python3
"""Validate the five independent Markdown documentation families.

No external dependencies; run from anywhere inside/outside the repository.
This is a structural/referential guard, not a substitute for a technical review.
"""

from __future__ import annotations

import glob
import re
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]

SCHEMAS = {
    "architecture": (
        "Status", "Purpose", "Scope", "Architecture model",
        "Runtime and lifecycle", "Verification strategy", "Design rationale",
        "Phase boundary", "References",
    ),
    "interview": ("Review scope", "Questions and answers", "Phase boundary", "References"),
    "phase": (
        "Status", "Objective", "Scope", "Deliverables", "Architecture and design",
        "Build and run", "Verification", "Evidence", "Interview focus",
        "Exit criteria", "References", "Next phase",
    ),
    "protocols": (
        "Status", "Purpose", "Scope", "Roles and terminology", "Wire model",
        "Runtime behavior", "Interactions and dependencies", "Failure modes",
        "Security considerations", "Verification observations", "Phase boundary", "References",
    ),
    "evidence": (
        "Status", "Verification scope", "Environment", "Automated verification",
        "Manual verification", "Artifacts", "Results", "Security and redaction",
        "Reproduction", "References",
    ),
    # Supporting evidence scenarios remain inside the evidence family, but
    # intentionally use a different schema from the phase-level index.
    "scenario": (
        "Purpose", "Preconditions", "Procedure", "Observations", "Result",
        "Artifacts", "Security and redaction", "References",
    ),
}

ERRORS: list[str] = []


def fail(path: Path, message: str) -> None:
    ERRORS.append(f"{path.relative_to(ROOT)}: {message}")


def sections(path: Path) -> tuple[str, dict[str, str], list[str], list[str]]:
    lines = path.read_text(encoding="utf-8").splitlines()
    headings: list[str] = []
    level1: list[str] = []
    chunks: dict[str, list[str]] = {}
    active: str | None = None
    fence: str | None = None
    for line in lines:
        fence_match = re.match(r"^\s*(`{3,}|~{3,})", line)
        if fence_match:
            sequence = fence_match.group(1)
            if fence is None:
                fence = sequence[0]
            elif sequence[0] == fence:
                fence = None
        if fence:
            continue
        if line.startswith("# "):
            level1.append(line)
        if line.startswith("## "):
            active = line[3:]
            headings.append(active)
            chunks.setdefault(active, [])
        elif active is not None:
            chunks[active].append(line)
    return lines[0] if lines else "", {k: "\n".join(v).strip() for k, v in chunks.items()}, headings, level1


def paths_exist(path: Path, body: str, *, parent: Path | None = None) -> None:
    for source in re.findall(r"`([^`\n]+)`", body):
        if not ("/" in source or source.endswith((".md", ".txt", ".log", ".hpp", ".cpp", ".sh"))):
            continue
        # Ignore command-like or placeholder tokens; only recognize repository
        # file references and short artifact names in their relevant sections.
        if " " in source or source.startswith(("--", "$", "http")):
            continue
        if source.startswith(("docs/", "evidence/", "scripts/", "src/", "tests/", "labs/", "config/", "cmake/", ".github/", "README")):
            candidate = ROOT / source
        elif parent is not None and "/" not in source:
            candidate = parent / source
        else:
            continue
        if "*" in source or "?" in source or "[" in source:
            if not glob.glob(str(candidate)):
                fail(path, f"reference has no target: `{source}`")
        elif not candidate.exists():
            fail(path, f"reference target missing: `{source}`")


def validate(path: Path, family: str, phase: int | None = None) -> None:
    title, bodies, found, h1s = sections(path)
    if family == "architecture":
        pattern = rf"# Phase {phase} Architecture — .+"
    elif family == "interview":
        pattern = rf"# Phase {phase} Interview Review — .+"
    elif family == "phase":
        pattern = rf"# Phase {phase} — .+"
    elif family == "evidence":
        pattern = rf"# Phase {phase} Evidence — .+"
    elif family == "scenario":
        pattern = rf"# Phase {phase} Evidence Scenario — .+"
    else:
        pattern = r"# [A-Za-z0-9.+/-]+ Protocol — .+"
    if len(h1s) != 1 or re.fullmatch(pattern, title) is None:
        fail(path, f"expected exactly one H1 matching {pattern!r}; got {h1s!r}")
    if tuple(found) != SCHEMAS[family]:
        fail(path, f"unexpected H2 schema: {found!r}; expected {SCHEMAS[family]!r}")
    for name in SCHEMAS[family]:
        if not bodies.get(name, ""):
            fail(path, f"required section is empty: {name}")
    if not bodies.get("References", "").startswith("- "):
        fail(path, "References must contain at least one list entry")
    paths_exist(path, bodies.get("References", ""))
    if family == "evidence":
        paths_exist(path, bodies.get("Artifacts", ""), parent=path.parent)
        if f"docs/phase{phase}/README.md" not in bodies.get("References", ""):
            fail(path, "References must point to the matching phase README")
    if family == "scenario":
        if f"evidence/phase{phase}/README.md" not in bodies.get("References", ""):
            fail(path, "scenario must reference the matching evidence phase index")
    if family == "phase":
        if f"evidence/phase{phase}/" not in path.read_text(encoding="utf-8"):
            fail(path, "phase README must link its evidence index or directory")
    if family == "interview":
        if "Distinguish design intent, observed behavior" not in bodies.get("Review scope", ""):
            fail(path, "Review scope must use the canonical evidence/intent qualifier")
        # A question must be inside the Q&A section and under a topic heading.
        raw = path.read_text(encoding="utf-8")
        question_section = raw.split("## Questions and answers\n", 1)[-1].split("\n## Phase boundary", 1)[0]
        questions = re.findall(r"^#### (.+)$", question_section, flags=re.MULTILINE)
        if not questions:
            fail(path, "at least one interview question is required")
        for q in questions:
            if not q.endswith("?"):
                fail(path, f"question heading must end in '?': {q!r}")
        if not re.search(r"^### .+", question_section, flags=re.MULTILINE):
            fail(path, "Q&A must group questions beneath a ### topic")
    if family in {"architecture", "phase", "evidence"} and phase is not None:
        status = bodies.get("Status", "")
        if "**Completed**" not in status and "**PENDING**" not in status:
            fail(path, "status must explicitly be **Completed** or **PENDING**")
        # These historical milestones are already tagged and accepted. Future
        # phases are allowed to remain pending without inventing a release tag.
        if phase <= 7 and "**Completed**" not in status:
            fail(path, "recorded Phase 0–7 milestone must remain **Completed**")


def main() -> int:
    paths = {
        "architecture": sorted((ROOT / "docs/architecture").glob("*.md")),
        "interview": sorted((ROOT / "docs/interview").glob("*.md")),
        "phase": sorted((ROOT / "docs").glob("phase*/README.md")),
        "protocols": sorted((ROOT / "docs/protocols").glob("*.md")),
        "evidence": sorted((ROOT / "evidence").glob("phase*/README.md")),
    }
    for family, files in paths.items():
        if not files:
            ERRORS.append(f"{family}: no Markdown files found")
        for path in files:
            if family == "phase" or family == "evidence":
                parent = path.parent.name
                match = re.fullmatch(r"phase([0-9]+)", parent)
            elif family == "interview":
                match = re.fullmatch(r"phase([0-9]+)-questions\.md", path.name)
            elif family == "architecture":
                content = path.read_text(encoding="utf-8").splitlines()[0]
                match = re.fullmatch(r"# Phase ([0-9]+) Architecture — .+", content)
            else:
                match = None
            if family != "protocols" and not match:
                fail(path, "unexpected phase file name or architecture H1")
                continue
            phase = int(match.group(1)) if match else None
            validate(path, family, phase)

    scenarios = sorted((ROOT / "evidence").glob("phase*/*/README.md"))
    for path in scenarios:
        parent_name = path.parent.parent.name
        match = re.fullmatch(r"phase([0-9]+)", parent_name)
        if not match:
            fail(path, "scenario must be under a phaseN directory")
            continue
        validate(path, "scenario", int(match.group(1)))

    phase_numbers = {int(p.parent.name[5:]) for p in paths["phase"] if re.fullmatch(r"phase[0-9]+", p.parent.name)}
    evidence_numbers = {int(p.parent.name[5:]) for p in paths["evidence"] if re.fullmatch(r"phase[0-9]+", p.parent.name)}
    if phase_numbers != evidence_numbers:
        ERRORS.append(f"phase/evidence README pairs differ: docs={sorted(phase_numbers)}, evidence={sorted(evidence_numbers)}")
    for n in sorted(phase_numbers & evidence_numbers):
        p_status = sections(ROOT / f"docs/phase{n}/README.md")[1].get("Status", "")
        e_status = sections(ROOT / f"evidence/phase{n}/README.md")[1].get("Status", "")
        p_done, e_done = "**Completed**" in p_status, "**Completed**" in e_status
        if p_done != e_done:
            ERRORS.append(f"phase{n}: phase/evidence milestone completion states disagree")
        if p_done:
            p_tag = re.search(r"milestone tag `(v[^`]+)`", p_status)
            e_tag = re.search(r"milestone tag `(v[^`]+)`", e_status)
            if not p_tag or not e_tag or p_tag.group(1) != e_tag.group(1):
                ERRORS.append(f"phase{n}: phase/evidence milestone tags disagree")
        if n == 8 and not p_done:
            # Readiness blockers are meaningful even though offline Phase 8
            # checks passed. These are current project facts, not a general
            # prohibition on future explicit acceptance.
            for ref in (
                ROOT / "docs/phase8/README.md",
                ROOT / "docs/architecture/threading-model.md",
                ROOT / "evidence/phase8/README.md",
            ):
                if not ref.exists():
                    ERRORS.append(f"phase8: missing boundary document: {ref.relative_to(ROOT)}")
                elif not all(label in ref.read_text(encoding="utf-8") for label in ("UNPROVEN", "BLOCKED")):
                    ERRORS.append(f"phase8: unresolved PJSUA2 boundary missing in {ref.relative_to(ROOT)}")
    if ERRORS:
        for item in ERRORS:
            print("FAIL:", item, file=sys.stderr)
        print(f"Documentation editorial verification: FAIL ({len(ERRORS)} issue(s))", file=sys.stderr)
        return 1
    print("Documentation editorial verification: PASS")
    for family, files in paths.items():
        suffix = f" (+ {len(scenarios)} scenario index(es))" if family == "evidence" else ""
        print(f"  {family:12} {len(files):2d} file(s){suffix}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
