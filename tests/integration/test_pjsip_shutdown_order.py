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
REGISTRATION_SOURCE = (
    'SipRegistrationClient',
    ROOT / 'src/pjsip/SipRegistrationClient.cpp',
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


def _one(body: str, marker: str, name: str) -> int:
    if body.count(marker) != 1:
        raise ValueError(f'{name}: expected one {marker}')
    return body.index(marker)


def verify_order(source: str, name: str) -> None:
    body = shutdown_body(source, name)

    account_close = _one(body, 'callback_context_->account_activity.close();', name)
    account_wait_marker = (
        'callback_context_->account_activity.wait_for_empty_for(std::chrono::seconds{2})'
    )
    if body.count(account_wait_marker) != 2:
        raise ValueError(f'{name}: expected two account callback drain waits')
    account_wait_before_call = body.index(account_wait_marker)

    hangup = _one(body, 'hangup();', name)
    disconnected = _one(body, 'wait_for_disconnection(std::chrono::seconds{2})', name)
    call_close = _one(body, 'callback_context_->call_activity.close();', name)
    call_wait = _one(
        body,
        'callback_context_->call_activity.wait_for_empty_for(std::chrono::seconds{2})',
        name,
    )
    slot_wait = _one(body, 'wait_for_call_cleanup(std::chrono::seconds{2})', name)
    call_reset = _one(body, 'call_.reset();', name)
    account_shutdown = _one(body, 'shutdown_account_safely()', name)
    account_wait_after_shutdown = body.index(account_wait_marker, account_shutdown)
    lib_destroy = _one(body, 'endpoint_->libDestroy()', name)
    account_reset = _one(body, 'account_.reset();', name)

    # stop() has a separate no-library cleanup branch, so endpoint_.reset()
    # legitimately occurs before the success path too. Require a reset after
    # libDestroy() rather than requiring exactly one reset in the whole body.
    endpoint_reset = body.find('endpoint_.reset();', lib_destroy)
    if endpoint_reset < 0:
        raise ValueError(f'{name}: endpoint reset missing after libDestroy')

    sequence = (
        account_close,
        account_wait_before_call,
        hangup,
        disconnected,
        call_close,
        call_wait,
        slot_wait,
        call_reset,
        account_shutdown,
        account_wait_after_shutdown,
        lib_destroy,
        account_reset,
        endpoint_reset,
    )
    if list(sequence) != sorted(sequence) or len(set(sequence)) != len(sequence):
        raise ValueError(f'{name}: cleanup must precede Call destruction')

    # These mutexes must guard the actual call_.reset() success-path block.
    protected_region = body[slot_wait:call_reset]
    for guard in ('incoming_callback_mutex_', 'call_callback_mutex_', 'call_object_mutex_'):
        if guard not in protected_region:
            raise ValueError(f'{name}: required callback lock missing: {guard}')




def verify_registration_order(source: str, name: str) -> None:
    body = shutdown_body(source, name)

    account_close = _one(body, 'callback_context_->account_activity.close();', name)
    account_wait_marker = (
        'callback_context_->account_activity.wait_for_empty_for(std::chrono::seconds{2})'
    )
    if body.count(account_wait_marker) != 2:
        raise ValueError(f'{name}: expected two account callback drain waits')

    account_wait_before_shutdown = body.index(account_wait_marker)
    account_shutdown = _one(body, 'shutdown_account_safely()', name)
    account_wait_after_shutdown = body.index(account_wait_marker, account_shutdown)
    lib_destroy = _one(body, 'endpoint_->libDestroy()', name)
    account_reset = _one(body, 'account_.reset();', name)

    endpoint_reset = body.find('endpoint_.reset();', account_reset)
    if endpoint_reset < 0:
        raise ValueError(f'{name}: endpoint reset missing after Account release')

    sequence = (
        account_close,
        account_wait_before_shutdown,
        account_shutdown,
        account_wait_after_shutdown,
        lib_destroy,
        account_reset,
        endpoint_reset,
    )
    if list(sequence) != sorted(sequence) or len(set(sequence)) != len(sequence):
        raise ValueError(f'{name}: Account target must survive through libDestroy')

    shutdown_guard = (
        'if (!shutdown_account_safely()) {\n'
        '        return;\n'
        '    }'
    )
    if shutdown_guard not in body:
        raise ValueError(f'{name}: account shutdown result must veto release')

class SipShutdownOrderTests(unittest.TestCase):
    def test_real_outgoing_order(self):
        name, path = SOURCES[0]
        verify_order(path.read_text(encoding='utf-8'), name)

    def test_real_incoming_order(self):
        name, path = SOURCES[1]
        verify_order(path.read_text(encoding='utf-8'), name)

    def test_real_registration_order(self):
        name, path = REGISTRATION_SOURCE
        verify_registration_order(path.read_text(encoding='utf-8'), name)

    def test_early_registration_account_release_rejected(self):
        name, path = REGISTRATION_SOURCE
        source = path.read_text(encoding='utf-8')
        old = (
            '    try {\n'
            '        endpoint_->libDestroy();\n'
            '    } catch (...) {\n'
            '        std::fputs("PJSIP cleanup failed during registration shutdown\\n", stderr);\n'
            '        return;\n'
            '    }\n\n'
            '    account_.reset();')
        new = (
            '    account_.reset();\n\n'
            '    try {\n'
            '        endpoint_->libDestroy();\n'
            '    } catch (...) {\n'
            '        std::fputs("PJSIP cleanup failed during registration shutdown\\n", stderr);\n'
            '        return;\n'
            '    }')
        self.assertEqual(source.count(old), 1)
        altered = source.replace(old, new, 1)
        with self.assertRaisesRegex(ValueError, 'survive through libDestroy'):
            verify_registration_order(altered, name)

    def test_early_call_destruction_rejected(self):
        name, path = SOURCES[0]
        source = path.read_text(encoding='utf-8')
        body = shutdown_body(source, name)
        cleanup_guard = (
            '    if (!wait_for_call_cleanup(std::chrono::seconds{2})) {\n'
            '        return;\n'
            '    }\n'
        )
        self.assertEqual(body.count(cleanup_guard), 1)
        altered = body.replace(cleanup_guard, '', 1).replace(
            '        call_.reset();',
            '        call_.reset();\n'
            '    if (!wait_for_call_cleanup(std::chrono::seconds{2})) { return; }',
            1,
        )
        self.assertNotEqual(body, altered)
        with self.assertRaisesRegex(ValueError, 'cleanup must precede'):
            verify_order(source.replace(body, altered, 1), name)

    def test_removed_cleanup_wait_rejected(self):
        name, path = SOURCES[1]
        source = path.read_text(encoding='utf-8')
        body = shutdown_body(source, name)
        cleanup_guard = (
            '    if (!wait_for_call_cleanup(std::chrono::seconds{2})) {\n'
            '        return;\n'
            '    }\n'
        )
        self.assertEqual(body.count(cleanup_guard), 1)
        altered = source.replace(cleanup_guard, '', 1)
        with self.assertRaisesRegex(ValueError, 'expected one'):
            verify_order(altered, name)

    def test_removed_callback_lock_rejected(self):
        name, path = SOURCES[0]
        source = path.read_text(encoding='utf-8')
        body = shutdown_body(source, name)
        lock = 'const std::lock_guard incoming_callback_lock{incoming_callback_mutex_};'
        self.assertEqual(body.count(lock), 1)
        altered = body.replace(lock, '/* callback lock deliberately removed */', 1)
        with self.assertRaisesRegex(ValueError, 'callback lock missing'):
            verify_order(source.replace(body, altered, 1), name)

    def test_duplicate_call_reset_rejected(self):
        name, path = SOURCES[1]
        source = path.read_text(encoding='utf-8')
        body = shutdown_body(source, name)
        self.assertEqual(body.count('call_.reset();'), 1)
        altered = body.replace('call_.reset();', 'call_.reset(); call_.reset();', 1)
        with self.assertRaisesRegex(ValueError, 'expected one'):
            verify_order(source.replace(body, altered, 1), name)


if __name__ == '__main__':
    unittest.main()
