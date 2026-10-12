#!/usr/bin/env python3
"""Fail-closed validator for allowlisted, non-secret P8-R3C live summary."""
from __future__ import annotations

from pathlib import Path
import sys

EXPECTED = {
    'P8R3C_BASELINE': '40cffc5',
    'P8R3C_VENDOR_REVISION': '5a457451',
    'P8R3C_VENDOR_BUILD': 'ISOLATED_STATIC_INSTRUMENTED',
    'P8R3C_REGISTRATION': 'PASS',
    'P8R3C_CALL_ATTEMPT': 'STARTED',
    'P8R3C_DISCONNECTION': 'PASS',
    'P8R3C_RELEASE_DISCONNECTED': 'RETURNED',
    'P8R3C_STOP': 'RETURNED',
    'P8R3C_DESTRUCTOR': 'RETURNED',
    'P8R3C_REAL_VENDOR_PRE_GATE_PAUSE': 'OBSERVED',
    'P8R3C_VENDOR_TO_APP_INGRESS': 'OBSERVED',
    'P8R3C_NO_RETIRE_WHILE_PAUSED': 'YES',
    'P8R3C_EXTERNAL_CALLBACK_QUIESCENCE': 'UNPROVEN',
    'P8R3C_INTEGRATION_READINESS': 'BLOCKED',
    'P8R3C_RESULT': 'PASS',
    'P8R3C_PROCESS_EXIT': '0',
}


def validate_summary(text: str) -> None:
    markers: dict[str, str] = {}
    for line in text.splitlines():
        if not line or line.count('=') != 1:
            raise ValueError('unexpected line in sanitized summary')
        key, value = line.split('=', 1)
        if key not in EXPECTED or key in markers:
            raise ValueError('unexpected or duplicate marker')
        markers[key] = value
    if markers != EXPECTED:
        raise ValueError('missing, mismatched, or inconclusive live observation')


def main() -> int:
    if len(sys.argv) != 2:
        print('Usage: verify-r3c-summary.py PATH', file=sys.stderr)
        return 2
    try:
        validate_summary(Path(sys.argv[1]).read_text(encoding='utf-8'))
    except (ValueError, OSError, UnicodeError) as error:
        print(f'P8-R3C live summary: FAIL ({type(error).__name__})', file=sys.stderr)
        return 1
    print('P8-R3C live summary: PASS (real dispatch observed, NOT quiescence)')
    return 0


if __name__ == '__main__':
    sys.exit(main())
