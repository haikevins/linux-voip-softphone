#!/usr/bin/env python3
"""Mutation regressions against the two *real* PJSUA2 shutdown clients."""

import importlib.util
import json
from pathlib import Path
import subprocess
import sys
import unittest

ROOT = Path(__file__).resolve().parents[2]
SCRIPT = ROOT / 'tests/integration/support/pjsip_shutdown_integration_audit.py'
spec = importlib.util.spec_from_file_location('pjsip_integration_blocker_audit', SCRIPT)
if spec is None or spec.loader is None:
    raise RuntimeError('cannot import Phase 8 integration contract')
audit = importlib.util.module_from_spec(spec)
sys.modules[spec.name] = audit
spec.loader.exec_module(audit)


class ShutdownIntegrationContractTest(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        base = ROOT / 'src/pjsip'
        cls.outgoing = (base / 'SipOutgoingCallClient.cpp').read_text(encoding='utf-8')
        cls.incoming = (base / 'SipIncomingCallClient.cpp').read_text(encoding='utf-8')
        cls.registration = (base / 'SipRegistrationClient.cpp').read_text(encoding='utf-8')
        cls.header = (base / 'PjsipShutdownAuthorization.hpp').read_text(encoding='utf-8')

    def evaluate(self, outgoing=None, incoming=None, registration=None, header=None):
        return audit.evaluate(
            self.outgoing if outgoing is None else outgoing,
            self.incoming if incoming is None else incoming,
            self.registration if registration is None else registration,
            self.header if header is None else header,
        )

    def changed(self, original, before, after):
        self.assertEqual(original.count(before), 1, 'mutation fixture anchor changed')
        return original.replace(before, after, 1)

    def test_real_clients_have_no_known_source_risks_but_remain_blocked(self):
        result = self.evaluate()
        self.assertEqual(result['audit_contract'], 'PASS')
        self.assertEqual(result['explicit_stop_hardening'], 'INTEGRATED')
        self.assertEqual(result['integration_readiness'], 'BLOCKED')
        self.assertEqual(result['destructor_failure_policy'], 'FAIL_CLOSED_TERMINATE')
        self.assertEqual(result['callback_owner_lifetime_policy'], 'SHARED_CONTEXT_DETACH')
        self.assertEqual(result['account_target_lifetime_policy'], 'RETAIN_THROUGH_LIBDESTROY')
        self.assertEqual(result['live_external_quiescence'], 'UNPROVEN')
        self.assertEqual(result['actual_pj_ebusy_recovery'], 'UNPROVEN')

    def test_both_clients_have_zero_known_source_closure_risks(self):
        result = self.evaluate()
        self.assertEqual(set(result['clients']), {'incoming', 'outgoing', 'registration'})
        for item in result['clients'].values():
            self.assertTrue(all(item['checks'].values()))
            self.assertEqual(item['open_risks'], [])
            self.assertEqual(item['unreviewed_tokens'], [])

    def test_four_proofs_in_header_are_independent_labels(self):
        report = self.evaluate()
        self.assertTrue(all(report['authorization_header_proofs'].values()))
        broken = self.changed(self.header, 'ExternalCallbackIngressQuiescent,',
                              'LocalCallbacksDrained,')
        report = self.evaluate(header=broken)
        self.assertEqual(report['audit_contract'], 'REVIEW_REQUIRED')
        self.assertEqual(report['integration_readiness'], 'REVIEW_REQUIRED')

    def test_comments_do_not_count_as_real_integration(self):
        outgoing = '// PjsipShutdownAuthorization ShutdownProof::ExternalCallbackIngressQuiescent\n'
        result = self.evaluate(outgoing=outgoing + self.outgoing)
        self.assertEqual(result['integration_readiness'], 'BLOCKED')

    def test_fake_external_quiescence_in_outgoing_stop_requires_review(self):
        changed = self.changed(
            self.outgoing, '        hangup();',
            '        hangup();\n'
            '        policy.report(generation, ShutdownProof::ExternalCallbackIngressQuiescent, '
            'ShutdownReceipt::Verified);',
        )
        result = self.evaluate(outgoing=changed)
        self.assertEqual(result['integration_readiness'], 'REVIEW_REQUIRED')
        self.assertIn('ShutdownProof::', result['clients']['outgoing']['unreviewed_tokens'])

    def test_local_callback_gate_cannot_supply_external_proof(self):
        changed = self.changed(
            self.incoming, '        hangup();',
            '        hangup();\n'
            '        if (local_callbacks_drained) { '
            'policy.report(generation, ShutdownProof::ExternalCallbackIngressQuiescent, '
            'ShutdownReceipt::Verified); }',
        )
        self.assertEqual(self.evaluate(incoming=changed)['audit_contract'], 'REVIEW_REQUIRED')

    def test_helper_include_without_provider_requires_review(self):
        changed = '#include "pjsip/PjsipShutdownAuthorization.hpp"\n' + self.incoming
        report = self.evaluate(incoming=changed)
        self.assertIn('PjsipShutdownAuthorization',
                      report['clients']['incoming']['unreviewed_tokens'])
        self.assertEqual(report['integration_readiness'], 'REVIEW_REQUIRED')

    def test_early_call_destruction_breaks_checked_stop_order(self):
        changed = self.changed(
            self.outgoing,
            '    if (!wait_for_call_cleanup(std::chrono::seconds{2})) {\n'
            '        return;\n'
            '    }',
            '    call_.reset();',
        )
        self.assertEqual(self.evaluate(outgoing=changed)['audit_contract'], 'REVIEW_REQUIRED')

    def test_account_result_discard_requires_review(self):
        changed = self.changed(
            self.incoming,
            '    if (!shutdown_account_safely()) {\n'
            '        return;\n'
            '    }',
            '    static_cast<void>(shutdown_account_safely());',
        )
        report = self.evaluate(incoming=changed)
        self.assertFalse(report['clients']['incoming']['checks'][
            'explicit_stop_result_and_gate_risks_resolved'])
        self.assertEqual(report['integration_readiness'], 'REVIEW_REQUIRED')

    def test_call_slot_result_discard_requires_review(self):
        changed = self.changed(
            self.outgoing,
            '    if (!wait_for_call_cleanup(std::chrono::seconds{2})) {\n'
            '        return;\n'
            '    }',
            '    static_cast<void>(wait_for_call_cleanup(std::chrono::seconds{2}));',
        )
        self.assertEqual(self.evaluate(outgoing=changed)['integration_readiness'],
                         'REVIEW_REQUIRED')

    def test_destructor_fail_closed_policy_removal_requires_fresh_review(self):
        changed = self.changed(
            self.outgoing,
            '    if (library_created_ || endpoint_ != nullptr || account_ != nullptr || call_ != nullptr) {\n'
            '        std::fputs("PJSIP outgoing client teardown could not prove safe destruction\\n", stderr);\n'
            '        std::terminate();\n'
            '    }',
            '',
        )
        report = self.evaluate(outgoing=changed)
        self.assertFalse(report['clients']['outgoing']['checks']['no_known_source_closure_risks'])
        self.assertEqual(report['integration_readiness'], 'REVIEW_REQUIRED')

    def test_callback_owner_detach_removal_requires_fresh_review(self):
        changed = self.changed(
            self.incoming, '    callback_context_->detach_owner();\n', '')
        report = self.evaluate(incoming=changed)
        self.assertFalse(report['clients']['incoming']['checks']['no_known_source_closure_risks'])
        self.assertEqual(report['integration_readiness'], 'REVIEW_REQUIRED')

    def test_callback_owner_lookup_before_admission_requires_review(self):
        before = (
            '        auto callback_entry = context_->account_activity.enter();\n'
            '        if (!callback_entry.admitted()) {\n'
            '            return;\n'
            '        }\n\n'
            '        auto *owner = context_->owner();\n'
            '        if (owner == nullptr) {\n'
            '            return;\n'
            '        }')
        after = (
            '        auto *owner = context_->owner();\n'
            '        auto callback_entry = context_->account_activity.enter();\n'
            '        if (!callback_entry.admitted() || owner == nullptr) {\n'
            '            return;\n'
            '        }')
        changed = self.changed(self.outgoing, before, after)
        report = self.evaluate(outgoing=changed)
        self.assertFalse(report['clients']['outgoing']['checks']['no_known_source_closure_risks'])
        self.assertEqual(report['integration_readiness'], 'REVIEW_REQUIRED')


    def test_registration_client_participates_in_blocker_audit(self):
        report = self.evaluate()
        registration = report['clients']['registration']
        self.assertTrue(all(registration['checks'].values()))
        self.assertEqual(registration['open_risks'], [])
        self.assertEqual(registration['unreviewed_tokens'], [])

    def test_early_registration_account_release_requires_review(self):
        before = (
            '    try {\n'
            '        endpoint_->libDestroy();\n'
            '    } catch (...) {\n'
            '        std::fputs("PJSIP cleanup failed during registration shutdown\\n", stderr);\n'
            '        return;\n'
            '    }\n\n'
            '    account_.reset();')
        after = (
            '    account_.reset();\n\n'
            '    try {\n'
            '        endpoint_->libDestroy();\n'
            '    } catch (...) {\n'
            '        std::fputs("PJSIP cleanup failed during registration shutdown\\n", stderr);\n'
            '        return;\n'
            '    }')
        changed = self.changed(self.registration, before, after)
        report = self.evaluate(registration=changed)
        self.assertFalse(
            report['clients']['registration']['checks']['no_known_source_closure_risks'])
        self.assertEqual(report['integration_readiness'], 'REVIEW_REQUIRED')

    def test_registration_raw_owner_reference_requires_review(self):
        before = (
            '  private:\n'
            '    std::shared_ptr<CallbackContext> context_;\n'
            '};')
        after = (
            '  private:\n'
            '    std::shared_ptr<CallbackContext> context_;\n'
            '    SipRegistrationClient &owner_;\n'
            '};')
        changed = self.changed(self.registration, before, after)
        report = self.evaluate(registration=changed)
        self.assertFalse(
            report['clients']['registration']['checks']['no_known_source_closure_risks'])
        self.assertEqual(report['integration_readiness'], 'REVIEW_REQUIRED')

    def test_json_cli_does_not_report_ready(self):
        result = subprocess.run(
            [sys.executable, '-B', str(SCRIPT), '--source-root', str(ROOT), '--json'],
            text=True, capture_output=True, check=False,
        )
        self.assertEqual(result.returncode, 0, result.stderr)
        report = json.loads(result.stdout)
        self.assertEqual(report['explicit_stop_hardening'], 'INTEGRATED')
        self.assertEqual(report['integration_readiness'], 'BLOCKED')
        self.assertEqual(report['destructor_failure_policy'], 'FAIL_CLOSED_TERMINATE')
        self.assertEqual(report['callback_owner_lifetime_policy'], 'SHARED_CONTEXT_DETACH')
        self.assertEqual(report['account_target_lifetime_policy'], 'RETAIN_THROUGH_LIBDESTROY')
        self.assertEqual(report['audit_contract'], 'PASS')

    def test_require_ready_cli_fails_closed(self):
        result = subprocess.run(
            [sys.executable, '-B', str(SCRIPT), '--source-root', str(ROOT), '--require-ready'],
            text=True, capture_output=True, check=False,
        )
        self.assertEqual(result.returncode, 3, result.stderr)
        self.assertIn('Integration readiness: BLOCKED', result.stdout)
        self.assertIn('Destructor failure policy: FAIL_CLOSED_TERMINATE', result.stdout)
        self.assertIn('Callback owner lifetime policy: SHARED_CONTEXT_DETACH', result.stdout)
        self.assertIn('Account target lifetime policy: RETAIN_THROUGH_LIBDESTROY', result.stdout)


if __name__ == '__main__':
    unittest.main(verbosity=2)
