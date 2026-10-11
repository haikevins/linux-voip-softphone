#!/usr/bin/env python3
"""P8-R3A: offline source contract for production callback target entry.

This is deliberately *not* a PJSUA2 external callback quiescence test.
It reviews known override methods and fails if the override set drifts.
"""
from __future__ import annotations

from pathlib import Path
import re
import unittest

ROOT = Path(__file__).resolve().parents[2]
CLIENTS = {
    'SipRegistrationClient': {'onRegState': 'account'},
    'SipOutgoingCallClient': {
        'onRegState': 'account',
        'onIncomingCall': 'account',
        'onCallState': 'call',
        'onCallMediaState': 'call',
    },
    'SipIncomingCallClient': {
        'onRegState': 'account',
        'onIncomingCall': 'account',
        'onCallState': 'call',
        'onCallMediaState': 'call',
    },
}
OVERRIDE = re.compile(r'\bvoid\s+(on\w+)\s*\([^)]*\)\s+override\s*\{')


def balanced_body(source: str, opening: int) -> str:
    """Extract a known C++ body (structural check, not a general C++ parser)."""
    if source[opening] != '{':
        raise ValueError('expected opening brace')
    depth = 0
    for index in range(opening, len(source)):
        if source[index] == '{':
            depth += 1
        elif source[index] == '}':
            depth -= 1
            if depth == 0:
                return source[opening + 1:index]
    raise ValueError('unterminated callback body')


def callback_bodies(source: str) -> dict[str, str]:
    result: dict[str, str] = {}
    for match in OVERRIDE.finditer(source):
        name = match.group(1)
        if name in result:
            raise ValueError(f'duplicate callback override: {name}')
        result[name] = balanced_body(source, match.end() - 1)
    return result


def check_callback(body: str, name: str, category: str) -> None:
    entry = f'auto callback_entry = context_->{category}_activity.enter();'
    rejection = 'if (!callback_entry.admitted()) {'
    owner_load = 'auto *owner = context_->owner();'
    null_guard = 'if (owner == nullptr) {'
    for marker in (entry, rejection, owner_load, null_guard):
        if body.count(marker) != 1:
            raise ValueError(f'{name}: missing or duplicate {marker}')
    if not (body.index(entry) < body.index(rejection) <
            body.index(owner_load) < body.index(null_guard)):
        raise ValueError(f'{name}: owner access precedes gated entry and null guard')
    rejection_body = balanced_body(body, body.index('{', body.index(rejection)))
    if 'return;' not in rejection_body:
        raise ValueError(f'{name}: rejected callback does not return')
    null_body = balanced_body(body, body.index('{', body.index(null_guard)))
    if 'return;' not in null_body:
        raise ValueError(f'{name}: null owner does not return')
    if 'owner->' in body[:body.index(null_guard) + len(null_guard)]:
        raise ValueError(f'{name}: owner dereferenced before null guard')
    # Both cases must keep the RAII callback_entry in scope through the method.
    if 'callback_entry.release()' in body:
        raise ValueError(f'{name}: callback permit released before callback return')


def check_client(source: str, header: str, name: str) -> None:
    actual = callback_bodies(source)
    expected = CLIENTS[name]
    if actual.keys() != expected.keys():
        raise ValueError(f'{name}: callback override set changed; explicit review required '
                         f'(actual={sorted(actual)}, expected={sorted(expected)})')
    if source.count('std::shared_ptr<CallbackContext> context_;') != (
        1 if name == 'SipRegistrationClient' else 2
    ):
        raise ValueError(f'{name}: derived callback targets must own shared context')
    if header.count('std::shared_ptr<CallbackContext> callback_context_;') != 1:
        raise ValueError(f'{name}: client must own shared callback context')
    if 'std::atomic<' + name + ' *> owner_;' not in source:
        raise ValueError(f'{name}: context owner detach field missing')
    for callback_name, category in expected.items():
        check_callback(actual[callback_name], callback_name, category)

    # Destruction must run stop() and apply the fail-closed resource check
    # before detaching the owner (no detached owner while the library lives).
    destructor = re.search(rf'{name}::~{name}\(\)\s*\{{', source)
    if destructor is None:
        raise ValueError(f'{name}: destructor missing')
    body = balanced_body(source, destructor.end() - 1)
    retained = 'library_created_ || endpoint_ != nullptr || account_ != nullptr'
    if not (body.count('stop();') == 1 and retained in body and
            body.count('std::terminate();') == 1 and
            body.count('callback_context_->detach_owner();') == 1 and
            body.index('stop();') < body.index(retained) <
            body.index('std::terminate();') < body.index('callback_context_->detach_owner();')):
        raise ValueError(f'{name}: fail-closed destructor/detach order changed')

    # A successful libDestroy() must precede release of the Account target;
    # callback ingress after Call destruction remains a separate vendor question.
    stop = re.search(rf'void {name}::stop\(\) noexcept\s*\{{', source)
    if stop is None:
        raise ValueError(f'{name}: stop missing')
    stop_body = balanced_body(source, stop.end() - 1)
    destroy = stop_body.find('endpoint_->libDestroy();')
    account_reset = stop_body.find('account_.reset();')
    if destroy < 0 or account_reset < destroy:
        raise ValueError(f'{name}: Account released before libDestroy')
    if 'callback_context_->detach_owner();' in stop_body:
        raise ValueError(f'{name}: owner detached during stop')


class Phase8R3CallbackTargetContract(unittest.TestCase):
    def test_all_real_callback_targets_have_gated_entry(self):
        for name in CLIENTS:
            with self.subTest(client=name):
                source = (ROOT / f'src/pjsip/{name}.cpp').read_text(encoding='utf-8')
                header = (ROOT / f'src/pjsip/{name}.hpp').read_text(encoding='utf-8')
                check_client(source, header, name)

    def test_callback_override_expansion_requires_review(self):
        source = (ROOT / 'src/pjsip/SipRegistrationClient.cpp').read_text(encoding='utf-8')
        header = (ROOT / 'src/pjsip/SipRegistrationClient.hpp').read_text(encoding='utf-8')
        altered = source.replace('  private:\n    std::shared_ptr<CallbackContext> context_;',
                                 '    void onNewCallback() override {}\n\n  private:\n'
                                 '    std::shared_ptr<CallbackContext> context_;', 1)
        self.assertNotEqual(altered, source)
        with self.assertRaisesRegex(ValueError, 'callback override set changed'):
            check_client(altered, header, 'SipRegistrationClient')

    def test_owner_read_moved_before_gate_is_rejected(self):
        name = 'SipRegistrationClient'
        source = (ROOT / f'src/pjsip/{name}.cpp').read_text(encoding='utf-8')
        header = (ROOT / f'src/pjsip/{name}.hpp').read_text(encoding='utf-8')
        entry = 'auto callback_entry = context_->account_activity.enter();'
        load = 'auto *owner = context_->owner();'
        # Replace the complete callback-entry sequence to model a genuine
        # owner read above the gate, then require the audit to reject it.
        original = (f'{entry}\n        if (!callback_entry.admitted()) {{\n'
                    '            return;\n        }\n\n        '
                    f'{load}')
        reordered = (f'{load}\n        {entry}\n'
                     '        if (!callback_entry.admitted()) {\n'
                     '            return;\n        }')
        self.assertIn(original, source)
        altered = source.replace(original, reordered, 1)
        with self.assertRaisesRegex(ValueError, 'owner access precedes'):
            check_client(altered, header, name)

    def test_rejected_callback_without_return_is_rejected(self):
        name = 'SipRegistrationClient'
        source = (ROOT / f'src/pjsip/{name}.cpp').read_text(encoding='utf-8')
        header = (ROOT / f'src/pjsip/{name}.hpp').read_text(encoding='utf-8')
        altered = source.replace('if (!callback_entry.admitted()) {\n            return;\n        }',
                                 'if (!callback_entry.admitted()) {\n        }', 1)
        self.assertNotEqual(altered, source)
        with self.assertRaisesRegex(ValueError, 'rejected callback does not return'):
            check_client(altered, header, name)

    def test_early_owner_detach_is_rejected(self):
        name = 'SipRegistrationClient'
        source = (ROOT / f'src/pjsip/{name}.cpp').read_text(encoding='utf-8')
        header = (ROOT / f'src/pjsip/{name}.hpp').read_text(encoding='utf-8')
        altered = source.replace('    stop();\n    if (library_created_',
                                 '    callback_context_->detach_owner();\n    stop();\n'
                                 '    if (library_created_', 1)
        self.assertNotEqual(altered, source)
        with self.assertRaisesRegex(ValueError, 'fail-closed destructor/detach order'):
            check_client(altered, header, name)


if __name__ == '__main__':
    unittest.main()
