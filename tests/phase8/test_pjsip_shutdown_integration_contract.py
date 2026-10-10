#!/usr/bin/env python3
"""Mutation regressions against the two *real* PJSUA2 shutdown clients."""

import importlib.util
import json
from pathlib import Path
import subprocess
import sys
import unittest

ROOT = Path(__file__).resolve().parents[2]
SCRIPT = ROOT / 'scripts/phase8/audit-pjsip-shutdown-integration.py'
spec = importlib.util.spec_from_file_location('phase8_integration_blocker_audit', SCRIPT)
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
        cls.header = (base / 'PjsipShutdownAuthorization.hpp').read_text(encoding='utf-8')

    def evaluate(self, outgoing=None, incoming=None, header=None):
        return audit.evaluate(
            self.outgoing if outgoing is None else outgoing,
            self.incoming if incoming is None else incoming,
            self.header if header is None else header,
        )

    def changed(self, original, before, after):
        self.assertEqual(original.count(before), 1, 'mutation fixture anchor changed')
        return original.replace(before, after, 1)

    def test_real_clients_are_explicitly_blocked_not_authorized(self):
        result = self.evaluate()
        self.assertEqual(result['audit_contract'], 'PASS')
        self.assertEqual(result['integration_readiness'], 'BLOCKED')
        self.assertEqual(result['live_external_quiescence'], 'UNPROVEN')
        self.assertEqual(result['actual_pj_ebusy_recovery'], 'UNPROVEN')

    def test_both_clients_retain_all_six_known_risks(self):
        result = self.evaluate()
        self.assertEqual(set(result['clients']), {'incoming', 'outgoing'})
        for item in result['clients'].values():
            self.assertTrue(all(item['checks'].values()))
            self.assertTrue(audit.EXPECTED_RISKS <= set(item['open_risks']))
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
            self.outgoing, '    hangup();',
            '    hangup();\n'
            '    policy.report(generation, ShutdownProof::ExternalCallbackIngressQuiescent, '
            'ShutdownReceipt::Verified);',
        )
        result = self.evaluate(outgoing=changed)
        self.assertEqual(result['integration_readiness'], 'REVIEW_REQUIRED')
        self.assertIn('ShutdownProof::', result['clients']['outgoing']['unreviewed_tokens'])

    def test_local_callback_gate_cannot_supply_external_proof(self):
        changed = self.changed(
            self.incoming, '    hangup();',
            '    hangup();\n'
            '    if (local_callbacks_drained) { '
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
            '    static_cast<void>(wait_for_call_cleanup(std::chrono::seconds{2}));',
            '    call_.reset();',
        )
        self.assertEqual(self.evaluate(outgoing=changed)['audit_contract'], 'REVIEW_REQUIRED')

    def test_account_result_handling_change_needs_fresh_review(self):
        changed = self.changed(self.incoming,
                               '    static_cast<void>(shutdown_account_safely());',
                               '    const bool success = shutdown_account_safely();')
        report = self.evaluate(incoming=changed)
        self.assertFalse(report['clients']['incoming']['checks'][
            'six_known_live_risks_still_identified'])
        self.assertEqual(report['integration_readiness'], 'REVIEW_REQUIRED')

    def test_call_slot_wait_result_change_needs_fresh_review(self):
        changed = self.changed(self.outgoing,
                               '    static_cast<void>(wait_for_call_cleanup(std::chrono::seconds{2}));',
                               '    const bool cleaned = wait_for_call_cleanup(std::chrono::seconds{2});')
        self.assertEqual(self.evaluate(outgoing=changed)['integration_readiness'],
                         'REVIEW_REQUIRED')

    def test_json_cli_does_not_report_ready(self):
        result = subprocess.run(
            [sys.executable, '-B', str(SCRIPT), '--source-root', str(ROOT), '--json'],
            text=True, capture_output=True, check=False,
        )
        self.assertEqual(result.returncode, 0, result.stderr)
        report = json.loads(result.stdout)
        self.assertEqual(report['integration_readiness'], 'BLOCKED')
        self.assertEqual(report['audit_contract'], 'PASS')

    def test_require_ready_cli_fails_closed(self):
        result = subprocess.run(
            [sys.executable, '-B', str(SCRIPT), '--source-root', str(ROOT), '--require-ready'],
            text=True, capture_output=True, check=False,
        )
        self.assertEqual(result.returncode, 3, result.stderr)
        self.assertIn('Integration readiness: BLOCKED', result.stdout)


if __name__ == '__main__':
    unittest.main(verbosity=2)
