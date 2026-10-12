#!/usr/bin/env python3
"""P8-R3B: fail closed if production Call release/temporary sites drift.

Structural source regression, not C++ parsing or a PJSUA2 lifetime proof.
The four existing call_.reset() sites and four stack-local rejected Calls
all require explicit re-review if added, removed, or relocated.
"""
from __future__ import annotations

from pathlib import Path
import re
import unittest

ROOT = Path(__file__).resolve().parents[2]
PJSIP = ROOT / 'src/pjsip'

RESETS = {
    'SipOutgoingCallClient': ('stop', 'make_call', 'release_disconnected_call'),
    'SipIncomingCallClient': ('stop',),
    'SipRegistrationClient': (),
}
TEMPORARY_CALLS = {
    'SipOutgoingCallClient': 2,
    'SipIncomingCallClient': 2,
    'SipRegistrationClient': 0,
}


def body_at(source: str, opening: int) -> str:
    if source[opening] != '{':
        raise ValueError('opening brace not found')
    depth = 0
    for offset in range(opening, len(source)):
        if source[offset] == '{':
            depth += 1
        elif source[offset] == '}':
            depth -= 1
            if not depth:
                return source[opening + 1:offset]
    raise ValueError('unterminated C++ function')


def member_body(source: str, name: str, method: str) -> str:
    header = re.search(rf'\b(?:void|bool)\s+{name}::{method}\s*\([^)]*\)'
                       r'\s*(?:noexcept\s*)?\{', source)
    if not header:
        raise ValueError(f'{name}::{method} signature missing')
    return body_at(source, header.end() - 1)


def check_inventory(sources: dict[str, str]) -> None:
    for name, methods in RESETS.items():
        source = sources[name]
        expected = len(methods)
        actual = source.count('call_.reset();')
        if actual != expected:
            raise ValueError(f'{name}: call_.reset inventory changed ({actual} != {expected})')
        for method in methods:
            body = member_body(source, name, method)
            if body.count('call_.reset();') != 1:
                raise ValueError(f'{name}::{method}: missing or additional call_.reset')

        temporary_names = re.findall(r'\bpj::Call\s+([a-zA-Z_][a-zA-Z_0-9]*)\s*\{',
                                     source)
        temporary = len(temporary_names)
        if temporary != TEMPORARY_CALLS[name] or any(
            variable != 'rejected_call' for variable in temporary_names
        ):
            raise ValueError(f'{name}: temporary rejected Call inventory changed')
        if temporary and source.count('rejected_call.answer(busy_parameter);') != temporary:
            raise ValueError(f'{name}: temporary rejected Call handling changed')

    outgoing = sources['SipOutgoingCallClient']
    release = member_body(outgoing, 'SipOutgoingCallClient', 'release_disconnected_call')
    if 'wait_for_call_cleanup(std::chrono::seconds{2})' not in release:
        raise ValueError('outgoing release: vendor call-slot wait changed')
    if 'call_activity.close()' in release:
        raise ValueError('outgoing release: call activity gate closure changed; review needed')
    if not release.index('wait_for_call_cleanup(') < release.index('call_.reset();'):
        raise ValueError('outgoing release: call-slot wait no longer precedes reset')
    # An intentionally narrow check: no conclusion about vendor pre-gate users.


class CallReleaseInventoryTest(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.sources = {
            name: (PJSIP / f'{name}.cpp').read_text(encoding='utf-8')
            for name in RESETS
        }

    def test_real_production_inventory(self):
        check_inventory(self.sources)

    def test_new_unreviewed_reset_is_rejected(self):
        changed = dict(self.sources)
        changed['SipOutgoingCallClient'] += '\ncall_.reset();\n'
        with self.assertRaisesRegex(ValueError, 'inventory changed'):
            check_inventory(changed)

    def test_removed_error_cleanup_reset_is_rejected(self):
        changed = dict(self.sources)
        old = member_body(changed['SipOutgoingCallClient'],
                          'SipOutgoingCallClient', 'make_call')
        self.assertIn('call_.reset();', old)
        # Unique position in the catch block of make_call.
        changed['SipOutgoingCallClient'] = changed['SipOutgoingCallClient'].replace(
            '    } catch (...) {\n        call_.reset();\n        throw;\n    }',
            '    } catch (...) {\n        throw;\n    }', 1)
        with self.assertRaisesRegex(ValueError, 'inventory changed'):
            check_inventory(changed)

    def test_new_temporary_rejected_call_is_rejected(self):
        changed = dict(self.sources)
        changed['SipIncomingCallClient'] += '\npj::Call rejected_call{account, call_id};\n'
        with self.assertRaisesRegex(ValueError, 'temporary rejected Call'):
            check_inventory(changed)


if __name__ == '__main__':
    unittest.main()
