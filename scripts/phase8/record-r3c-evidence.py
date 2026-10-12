#!/usr/bin/env python3
"""Record only a validated, bounded P8-R3C live summary, never raw SIP logs."""
from __future__ import annotations

from datetime import datetime, timezone
from pathlib import Path
import runpy
import sys

ROOT = Path(__file__).resolve().parents[2]


def main() -> int:
    if len(sys.argv) != 2:
        print('Usage: record-r3c-evidence.py SANITIZED_SUMMARY_PATH', file=sys.stderr)
        return 2
    validator = runpy.run_path(str(ROOT / 'scripts/phase8/verify-r3c-summary.py'))
    source = Path(sys.argv[1])
    try:
        if source.is_symlink() or not source.is_file() or source.stat().st_size > 4096:
            raise ValueError('summary must be a regular, small, non-symlink file')
        content = source.read_text(encoding='utf-8')
        validator['validate_summary'](content)
        name = datetime.now(timezone.utc).strftime('p8-r3c-live-%Y%m%dT%H%M%SZ.txt')
        destination = ROOT / 'evidence/phase8' / name
        # 'x' means no overwrites, including a second run in the same second.
        with destination.open('x', encoding='utf-8') as target:
            target.write(content)
    except (OSError, UnicodeError, ValueError):
        print('ABORT: summary is invalid or evidence destination already exists', file=sys.stderr)
        return 1
    print(f'P8R3C_RECORDED_EVIDENCE={destination.relative_to(ROOT)}')
    print('P8R3C_EXTERNAL_CALLBACK_QUIESCENCE=UNPROVEN')
    return 0


if __name__ == '__main__':
    sys.exit(main())
