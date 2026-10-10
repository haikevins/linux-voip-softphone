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
            self.assertGreaterEqual(len(item['checks']), 20)
            self.assertTrue(all(item['checks'].values()))

    def test_both_clients_retain_only_two_closure_risks(self):
        result = self.run_audit()
        expected = {
            'ACCOUNT_DESTROYED_AFTER_POSSIBLE_SHUTDOWN_FAILURE',
            'CALLBACKS_BORROW_OWNER_BY_REFERENCE',
        }
        resolved = {
            'ACCOUNT_SHUTDOWN_RESULT_DISCARDED',
            'CALL_SLOT_WAIT_RESULT_DISCARDED',
            'NO_PRODUCTION_CALLBACK_ACTIVITY_GATE',
            'REGISTRATION_CALLBACK_NO_TEARDOWN_GATE',
        }
        for item in result['clients'].values():
            self.assertEqual(expected, set(item['risks']))
            self.assertTrue(resolved.isdisjoint(item['risks']))

    def test_call_slot_result_must_veto_call_release(self):
        old = ('    if (!wait_for_call_cleanup(std::chrono::seconds{2})) {\n'
               '        return;\n'
               '    }')
        modified = self.replace_once(
            self.original_outgoing, old,
            '    static_cast<void>(wait_for_call_cleanup(std::chrono::seconds{2}));')
        result = self.run_audit(outgoing=modified)
        self.assertFalse(result['clients']['outgoing']['checks']['slot_failure_vetoes_call_release'])
        self.assertIn('CALL_SLOT_WAIT_RESULT_DISCARDED', result['clients']['outgoing']['risks'])
        self.assertEqual('FAIL', result['source_contract'])

    def test_account_shutdown_result_must_veto_account_release(self):
        old = ('    if (!shutdown_account_safely()) {\n'
               '        return;\n'
               '    }')
        modified = self.replace_once(
            self.original_incoming, old, '    static_cast<void>(shutdown_account_safely());')
        result = self.run_audit(incoming=modified)
        self.assertFalse(
            result['clients']['incoming']['checks']['account_failure_vetoes_account_release'])
        self.assertIn('ACCOUNT_SHUTDOWN_RESULT_DISCARDED', result['clients']['incoming']['risks'])
        self.assertEqual('FAIL', result['source_contract'])

    def test_callback_gate_and_mutex_regressions_are_rejected(self):
        reg_old = (
            '    void onRegState(pj::OnRegStateParam &parameter) override\n'
            '    {\n'
            '        auto callback_entry = owner_.account_callback_activity_.enter();\n'
            '        if (!callback_entry.admitted()) {\n'
            '            return;\n'
            '        }\n\n')
        reg_new = '    void onRegState(pj::OnRegStateParam &parameter) override\n    {\n'
        no_reg_gate = self.replace_once(self.original_outgoing, reg_old, reg_new)
        result = self.run_audit(outgoing=no_reg_gate)
        self.assertFalse(result['clients']['outgoing']['checks']['registration_callback_gate'])
        self.assertIn('REGISTRATION_CALLBACK_NO_TEARDOWN_GATE',
                      result['clients']['outgoing']['risks'])

        no_incoming_lock = self.replace_once(
            self.original_incoming,
            'const std::lock_guard callback_lock{owner_.incoming_callback_mutex_};',
            '/* incoming callback lock removed */')
        self.assertFalse(
            self.run_audit(incoming=no_incoming_lock)['clients']['incoming']['checks'][
                'incoming_callback_mutex'])

        media_old = (
            'void onCallMediaState(pj::OnCallMediaStateParam & /*parameter*/) override\n'
            '    {\n'
            '        auto callback_entry = owner_.call_callback_activity_.enter();\n'
            '        if (!callback_entry.admitted()) {\n'
            '            return;\n'
            '        }\n\n'
            '        const std::lock_guard callback_lock{owner_.call_callback_mutex_};')
        media_new = (
            'void onCallMediaState(pj::OnCallMediaStateParam & /*parameter*/) override\n'
            '    {\n'
            '        auto callback_entry = owner_.call_callback_activity_.enter();\n'
            '        if (!callback_entry.admitted()) {\n'
            '            return;\n'
            '        }')
        no_media_lock = self.replace_once(self.original_outgoing, media_old, media_new)
        self.assertFalse(
            self.run_audit(outgoing=no_media_lock)['clients']['outgoing']['checks'][
                'media_callback_mutex'])

    def test_closed_account_gate_must_reject_new_incoming_call(self):
        old = ('                pj::Call rejected_call{*this, parameter.callId};\n'
               '                pj::CallOpParam busy_parameter;\n'
               '                busy_parameter.statusCode = PJSIP_SC_BUSY_HERE;\n'
               '                rejected_call.answer(busy_parameter);')
        modified = self.replace_once(self.original_outgoing, old, '                return;')
        result = self.run_audit(outgoing=modified)
        self.assertFalse(
            result['clients']['outgoing']['checks']['incoming_rejects_after_gate_close'])
        self.assertEqual('FAIL', result['source_contract'])

    def test_nonforced_shutdown_and_real_call_slot_counter_are_required(self):
        forced = self.replace_once(self.original_outgoing,
                                   'parameter.force = false;', 'parameter.force = true;')
        self.assertFalse(
            self.run_audit(outgoing=forced)['clients']['outgoing']['checks'][
                'non_forced_account_shutdown'])

        fake_slots = self.replace_once(self.original_incoming,
                                       'timeout, [] { return pjsua_call_get_count(); },',
                                       'timeout, [] { return 0U; },')
        self.assertFalse(
            self.run_audit(incoming=fake_slots)['clients']['incoming']['checks'][
                'polling_uses_real_call_slots'])

    def test_restart_requires_drained_gates_and_no_retained_teardown_state(self):
        no_gate_reset = self.replace_once(
            self.original_outgoing,
            '    if (!account_callback_activity_.reset_if_empty() ||\n'
            '        !call_callback_activity_.reset_if_empty()) {\n'
            '        throw std::logic_error("SIP outgoing callback activity did not drain before restart");\n'
            '    }\n',
            '')
        self.assertFalse(
            self.run_audit(outgoing=no_gate_reset)['clients']['outgoing']['checks'][
                'restart_requires_empty_callback_gates'])

        no_retained_check = self.replace_once(
            self.original_incoming,
            '    if (library_created_ || endpoint_ != nullptr || account_ != nullptr || call_ != nullptr) {\n'
            '        throw std::logic_error("SIP incoming call client retains teardown state");\n'
            '    }\n',
            '')
        self.assertFalse(
            self.run_audit(incoming=no_retained_check)['clients']['incoming']['checks'][
                'restart_rejects_retained_teardown_state'])

    def test_missing_callback_override_is_rejected(self):
        modified = self.replace_once(
            self.original_incoming,
            'void onCallState(pj::OnCallStateParam & /*parameter*/) override',
            'void renamedCallState(pj::OnCallStateParam & /*parameter*/) override')
        result = self.run_audit(incoming=modified)
        self.assertEqual('FAIL', result['source_contract'])
        self.assertEqual(['STRUCTURE_UNRECOGNIZED'], result['clients']['incoming']['risks'])


if __name__ == '__main__':
    unittest.main(verbosity=2)
