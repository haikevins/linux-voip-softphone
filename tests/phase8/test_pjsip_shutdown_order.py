#!/usr/bin/env python3
"""Structural regression guard for PJSUA2 client shutdown ordering.

Not a dynamic callback-lifetime test. Uses standard library only and does not
import or instantiate PJSUA2. Intentionally avoids writing __pycache__.
"""
from __future__ import annotations

from pathlib import Path
import unittest

ROOT = Path(__file__).resolve().parents[2]
SOURCES = (
    ('SipOutgoingCallClient', ROOT / 'src/pjsip/SipOutgoingCallClient.cpp'),
    ('SipIncomingCallClient', ROOT / 'src/pjsip/SipIncomingCallClient.cpp'),
)


def shutdown_body(source: str, class_name: str) -> str:
    signature = f'void {class_name}::stop() noexcept'
    if source.count(signature) != 1:
        raise ValueError('expected one stop() definition')
    start = source.index(signature)
    start = source.index('{', start)
    level = 0
    for i in range(start, len(source)):
        if source[i] == '{':
            level += 1
        elif source[i] == '}':
            level -= 1
            if level == 0:
                return source[start + 1:i]
    raise ValueError('unterminated stop() definition')


def verify_order(source: str, name: str) -> None:
    body = shutdown_body(source, name)
    sequence = (
        'hangup();',
        'wait_for_call_cleanup(std::chrono::seconds{2})',
        'call_.reset();',
        'shutdown_account_safely()',
        'account_.reset();',
        'endpoint_->libDestroy()',
        'endpoint_.reset();',
    )
    positions = []
    for marker in sequence:
        if body.count(marker) != 1:
            raise ValueError(f'{name}: expected one {marker}')
        positions.append(body.index(marker))
    if positions != sorted(positions) or len(set(positions)) != len(positions):
        raise ValueError(f'{name}: cleanup must precede Call destruction')

    for guard in ('incoming_callback_mutex_', 'call_callback_mutex_', 'call_object_mutex_'):
        if body.count(guard) != 1:
            raise ValueError(f'{name}: required callback lock missing: {guard}')
        if body.index(guard) < positions[1] or body.index(guard) > positions[2]:
            raise ValueError(f'{name}: {guard} not guarding call_.reset()')


class SipShutdownOrderTests(unittest.TestCase):
    def test_real_outgoing_order(self):
        name, path = SOURCES[0]
        verify_order(path.read_text(encoding='utf-8'), name)

    def test_real_incoming_order(self):
        name, path = SOURCES[1]
        verify_order(path.read_text(encoding='utf-8'), name)

    def test_early_call_destruction_rejected(self):
        name, path = SOURCES[0]
        source = path.read_text(encoding='utf-8')
        body = shutdown_body(source, name)
        altered = body.replace(
            'static_cast<void>(wait_for_call_cleanup(std::chrono::seconds{2}));',
            '', 1,
        ).replace(
            'call_.reset();',
            'call_.reset();\n    static_cast<void>(wait_for_call_cleanup(std::chrono::seconds{2}));', 1,
        )
        self.assertNotEqual(body, altered)
        with self.assertRaisesRegex(ValueError, 'cleanup must precede'):
            verify_order(source.replace(body, altered, 1), name)

    def test_removed_cleanup_wait_rejected(self):
        name, path = SOURCES[1]
        source = path.read_text(encoding='utf-8')
        altered = source.replace(
            'static_cast<void>(wait_for_call_cleanup(std::chrono::seconds{2}));',
            '', 1,
        )
        with self.assertRaisesRegex(ValueError, 'expected one'):
            verify_order(altered, name)

    def test_removed_callback_lock_rejected(self):
        name, path = SOURCES[0]
        source = path.read_text(encoding='utf-8')
        body = shutdown_body(source, name)
        altered = body.replace('incoming_callback_mutex_', 'ELIDED_CALLBACK_MUTEX', 1)
        with self.assertRaisesRegex(ValueError, 'callback lock missing'):
            verify_order(source.replace(body, altered, 1), name)

    def test_duplicate_call_reset_rejected(self):
        name, path = SOURCES[1]
        source = path.read_text(encoding='utf-8')
        body = shutdown_body(source, name)
        altered = body.replace('call_.reset();', 'call_.reset(); call_.reset();', 1)
        with self.assertRaisesRegex(ValueError, 'expected one'):
            verify_order(source.replace(body, altered, 1), name)


if __name__ == '__main__':
    unittest.main()
