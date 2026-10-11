#!/usr/bin/env python3
"""Read-only source hotspot inventory for Phase 8 Qt/PJSIP owner audit.

This is a *locator*, not a verifier of thread correctness. Never prints
source text, matched text, secrets, or absolute paths.
"""

from __future__ import annotations

import argparse
from collections import Counter, defaultdict
from dataclasses import dataclass
from pathlib import Path
import re
import sys

SOURCE_SUFFIXES = frozenset({".cc", ".cpp", ".cxx", ".h", ".hh", ".hpp", ".qml"})
SOURCE_DIRS = ("src", "include", "app", "apps", "gui", "qml")

# Category IDs are intentionally coarse: an occurrence is NOT a defect.
PATTERNS = (
    ("PJSUA_CALLBACK", r"\b(?:onRegState|onIncomingCall|onCallState|onCallMediaState|onCallTsxState|onCallTransferRequest|onCallReplaceRequest|onTransportState|onCallSdpCreated)\s*\("),
    ("PJSUA_OBJECT", r"\b(?:pj::(?:Endpoint|Account|Call|AudioMedia)|pjsua2|libCreate|libInit|libStart|libDestroy|accRemove|makeCall|hangup)\b"),
    ("QT_QUEUED_HANDOFF", r"\b(?:QMetaObject::invokeMethod|QCoreApplication::postEvent|Qt::QueuedConnection|QTimer::singleShot)\b"),
    ("QT_DIRECT_OR_BLOCKING", r"\bQt::(?:DirectConnection|BlockingQueuedConnection)\b"),
    ("QT_THREAD_AFFINITY", r"\b(?:QThread::currentThread|QObject::thread|moveToThread|Q_ASSERT|qFatal)\b"),
    ("QT_SIGNAL_CONNECTION", r"\b(?:QObject::connect|QObject::disconnect|Q_PROPERTY|Q_OBJECT|emit)\b"),
    ("WORKER_LIFECYCLE", r"(?:\b(?:std::jthread|std::thread|request_stop|stop_token)\b|\.(?:join|detach)\s*\()"),
    ("LOCK_OR_HANDOFF", r"\b(?:std::mutex|std::shared_mutex|std::condition_variable|std::atomic|std::weak_ptr|QPointer)\b"),
    ("TEARDOWN", r"\b(?:shutdown|stopWorker|stopThread|request_close|deleteLater|joinWorker|closeEngine|stopEndpoint)\s*\("),
    ("CALL_STATE_BOUNDARY", r"\b(?:CallManager|QtRegistrationBackend|Pjsua2|RtpDiagnostics|setCallState|registrationState)\b"),
)
RULES = tuple((label, re.compile(regex)) for label, regex in PATTERNS)


@dataclass(frozen=True, order=True)
class Hit:
    relative_path: str
    line: int
    category: str


def discover_files(root: Path) -> list[Path]:
    found = []
    for directory in SOURCE_DIRS:
        base = root / directory
        if base.is_symlink() or not base.is_dir():
            continue
        for path in base.rglob("*"):
            if (path.is_file() and not path.is_symlink()
                    and path.suffix.lower() in SOURCE_SUFFIXES
                    and not any(part.startswith(".") for part in path.relative_to(root).parts)):
                found.append(path)
    return sorted(set(found), key=lambda p: p.relative_to(root).as_posix())


def inventory(root: Path) -> tuple[list[Path], list[Hit]]:
    files = discover_files(root)
    hits: list[Hit] = []
    for path in files:
        rel = path.relative_to(root).as_posix()
        # Read only; never echo file content, even on parsing errors.
        with path.open("r", encoding="utf-8", errors="replace") as source:
            for index, line in enumerate(source, 1):
                for category, regex in RULES:
                    if regex.search(line):
                        hits.append(Hit(rel, index, category))
    return files, hits


def render(files: list[Path], hits: list[Hit], limit: int) -> str:
    files_with_hits = {hit.relative_path for hit in hits}
    counts = Counter(hit.category for hit in hits)
    by_file: dict[str, set[str]] = defaultdict(set)
    for hit in hits:
        by_file[hit.relative_path].add(hit.category)

    lines = [
        "PHASE 8 COMMIT 7 — QT/PJSIP OWNERSHIP HOTSPOT INVENTORY",
        "STATUS: STATIC LOCATOR ONLY — ALL OWNERSHIP/LIFETIME FINDINGS PENDING MANUAL REVIEW",
        "No source lines, matched tokens, usernames, passwords, or absolute paths are emitted.",
        f"Scanned source files: {len(files)}",
        f"Source files with matches: {len(files_with_hits)}",
        f"Total category-line hits: {len(hits)}",
        "",
        "CATEGORY COUNTS (occurrences are not defects):",
    ]
    for category, _ in RULES:
        lines.append(f"  {category}: {counts[category]}")
    if counts["PJSUA_CALLBACK"] == 0 and counts["PJSUA_OBJECT"] == 0:
        lines.append("  WARNING: no PJSUA callback/object tokens matched; verify source coverage.")
    lines.extend(["", "REVIEW PRIORITY (category breadth, then path):"])
    ranked = sorted(by_file.items(), key=lambda item: (-len(item[1]), item[0]))
    for path, cats in ranked[:100]:
        lines.append(f"  {path}: {len(cats)} categories")
    if len(ranked) > 100:
        lines.append(f"  ... {len(ranked) - 100} more matching files")
    lines.extend(["", "LOCATIONS (no source text):"])
    for hit in hits[:limit]:
        lines.append(f"  {hit.relative_path}:{hit.line} {hit.category}")
    if len(hits) > limit:
        lines.append(f"  TRUNCATED {len(hits) - limit} hits; rerun with --max-hits {len(hits)}")
    lines.extend([
        "", "MANUAL REVIEW — NOT AUTOMATICALLY SATISFIED:",
        "  [ ] Determine actual PJSUA2 callback execution threads and thread registration.",
        "  [ ] Confirm GUI state changes and QObjects remain on their owning Qt thread.",
        "  [ ] Verify queued handoff target lifetime, weak guards, and shutdown ordering.",
        "  [ ] Verify no signal/callback executes after owner destruction or endpoint teardown.",
        "  [ ] Check CallManager owner-thread policy at every SIP callback entry.",
        "  [ ] Inspect shutdown while registration/call callbacks are in flight.",
        "  [ ] Confirm no PJSIP object is accessed from SerialTaskWorker without explicit ownership.",
        "  [ ] Run isolated SIP/GUI runtime checks before claiming app-level safety.",
        "VERDICT: INCONCLUSIVE until reviewed against canonical source and tests.",
    ])
    return "\n".join(lines) + "\n"


def main(argv: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--root", type=Path, default=Path(__file__).resolve().parents[2],
                        help="repository root (default: relative to script)")
    parser.add_argument("--max-hits", type=int, default=2000,
                        help="maximum source locations to emit (default 2000)")
    args = parser.parse_args(argv)
    if not 1 <= args.max_hits <= 200000:
        parser.error("--max-hits must be in [1, 200000]")
    root = args.root.resolve()
    if not root.is_dir():
        parser.error("repository root not found")
    try:
        files, hits = inventory(root)
    except OSError:
        print("BLOCKED: cannot read source files (no source text emitted)", file=sys.stderr)
        return 2
    if not files:
        print("BLOCKED: no application source files under configured app source roots", file=sys.stderr)
        return 2
    print(render(files, hits, args.max_hits), end="")
    return 0


if __name__ == "__main__":
    sys.exit(main())
