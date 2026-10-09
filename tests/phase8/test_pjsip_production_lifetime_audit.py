#!/usr/bin/env python3
"""Mutation-oriented source-contract tests; no SIP stack or credentials needed."""

import importlib.util
from pathlib import Path
import sys
import unittest

ROOT = Path(__file__).resolve().parents[2]
TOOL = ROOT / 'scripts/phase8/audit-pjsip-production-lifetime.py'
spec = importlib.util.spec_from_file_location('pjsip_lifetime_audit', TOOL)
if spec is None or spec.loader is None:
    raise RuntimeError('Unable to load production audit')
audit = importlib.util.module_from_spec(spec)
sys.modules[spec.name] = audit
spec.loader.exec_module(audit)


class ProductionCallbackLifetimeAuditTest(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.original_outgoing = (ROOT / 'src/pjsip/SipOutgoingCallClient.cpp').read_text()
        cls.original_incoming = (ROOT / 'src/pjsip/SipIncomingCallClient.cpp').read_text()

    def run_audit(self, outgoing=None, incoming=None):
        return audit.audit_pair(outgoing if outgoing is not None else self.original_outgoing,
                                incoming if incoming is not None else self.original_incoming)

    @staticmethod
    def replace_once(text, old, new):
        if text.count(old) != 1:
            raise AssertionError(f'expected one fixture anchor: {old!r}; found {text.count(old)}')
        return text.replace(old, new, 1)

    def test_real_production_sources_satisfy_checked_structure(self):
        result = self.run_audit()
        self.assertEqual('PASS', result['source_contract'])
        self.assertEqual('INCONCLUSIVE', result['live_callback_quiescence'])
        for item in result['clients'].values():
            self.assertEqual([], item['errors'])
            self.assertEqual(9, len(item['checks']))
            self.assertTrue(all(item['checks'].values()))

    def test_both_clients_report_unverified_account_shutdown(self):
        result = self.run_audit()
        for item in result['clients'].values():
            self.assertIn('ACCOUNT_SHUTDOWN_RESULT_DISCARDED', item['risks'])
            self.assertIn('ACCOUNT_DESTROYED_AFTER_POSSIBLE_SHUTDOWN_FAILURE', item['risks'])
            self.assertIn('CALL_SLOT_WAIT_RESULT_DISCARDED', item['risks'])
            self.assertIn('NO_PRODUCTION_CALLBACK_ACTIVITY_GATE', item['risks'])
            self.assertIn('CALLBACKS_BORROW_OWNER_BY_REFERENCE', item['risks'])
            self.assertIn('REGISTRATION_CALLBACK_NO_TEARDOWN_GATE', item['risks'])

    def test_reordered_outgoing_stop_is_rejected(self):
        text = self.original_outgoing
        text = self.replace_once(text, 'static_cast<void>(wait_for_call_cleanup(std::chrono::seconds{2}));',
                                 'call_.reset();')
        # One reset is now inserted early; a duplicate reset remains inside the mutex.
        result = self.run_audit(outgoing=text)
        self.assertFalse(result['clients']['outgoing']['checks']['shutdown_order'])
        self.assertFalse(result['clients']['outgoing']['checks']['bounded_wait_before_release'])

    def test_reordered_incoming_account_cleanup_is_rejected(self):
        old = 'static_cast<void>(shutdown_account_safely());\n    account_.reset();'
        new = 'account_.reset();\n    static_cast<void>(shutdown_account_safely());'
        modified = self.replace_once(self.original_incoming, old, new)
        self.assertFalse(self.run_audit(incoming=modified)['clients']['incoming']['checks']['shutdown_order'])

    def test_removed_incoming_callback_lock_is_rejected(self):
        modified = self.replace_once(self.original_incoming,
                                     'const std::lock_guard callback_lock{owner_.incoming_callback_mutex_};',
                                     '/* callback lock deliberately removed */')
        self.assertFalse(self.run_audit(incoming=modified)['clients']['incoming']['checks']['incoming_callback_mutex'])

    def test_removed_call_media_lock_is_rejected(self):
        text = self.original_outgoing
        # Scope precisely to the single tracked-call class callback.
        old = ('void onCallMediaState(pj::OnCallMediaStateParam & /*parameter*/) override\n'
               '    {\n        const std::lock_guard callback_lock{owner_.call_callback_mutex_};')
        new = ('void onCallMediaState(pj::OnCallMediaStateParam & /*parameter*/) override\n'
               '    {')
        modified = self.replace_once(text, old, new)
        self.assertFalse(self.run_audit(outgoing=modified)['clients']['outgoing']['checks']['media_callback_mutex'])

    def test_forced_account_shutdown_is_rejected(self):
        modified = self.replace_once(self.original_outgoing,
                                     'parameter.force = false;', 'parameter.force = true;')
        self.assertFalse(self.run_audit(outgoing=modified)['clients']['outgoing']['checks']['non_forced_account_shutdown'])

    def test_removed_call_slot_counter_is_rejected(self):
        modified = self.replace_once(self.original_incoming,
                                     'timeout, [] { return pjsua_call_get_count(); },',
                                     'timeout, [] { return 0U; },')
        self.assertFalse(self.run_audit(incoming=modified)['clients']['incoming']['checks']['polling_uses_real_call_slots'])

    def test_missing_callback_override_is_rejected(self):
        modified = self.replace_once(self.original_incoming,
                                     'void onCallState(pj::OnCallStateParam & /*parameter*/) override',
                                     'void renamedCallState(pj::OnCallStateParam & /*parameter*/) override')
        result = self.run_audit(incoming=modified)
        self.assertEqual('FAIL', result['source_contract'])
        self.assertEqual(['STRUCTURE_UNRECOGNIZED'], result['clients']['incoming']['risks'])


if __name__ == '__main__':
    unittest.main(verbosity=2)
