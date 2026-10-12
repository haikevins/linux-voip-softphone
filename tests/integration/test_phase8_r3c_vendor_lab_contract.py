#!/usr/bin/env python3
"""P8-R3C offline contract regression: no vendor/live claim without a real run."""
from __future__ import annotations

from pathlib import Path
import os
import runpy
import subprocess
import unittest

ROOT = Path(__file__).resolve().parents[2]
LAB = ROOT / 'labs/pjsip/callback-dispatch'
SCRIPT = ROOT / 'scripts/phase8/run-r3c-vendor-dispatch-lab.sh'
VALIDATOR = ROOT / 'scripts/phase8/verify-r3c-summary.py'
APP = ROOT / 'src/pjsip/SipOutgoingCallClient.cpp'
PATCH = LAB / 'pjproject-2.17-vendor-hook.patch'
VALIDATE = runpy.run_path(str(VALIDATOR))['validate_summary']
EXPECTED = runpy.run_path(str(VALIDATOR))['EXPECTED']


def valid_summary() -> str:
    return '\n'.join(f'{key}={value}' for key, value in EXPECTED.items()) + '\n'


class VendorLabContractTest(unittest.TestCase):
    def test_summary_pass_and_fail_closed_mutations(self):
        VALIDATE(valid_summary())
        for key, alternative in [
            ('P8R3C_REAL_VENDOR_PRE_GATE_PAUSE', 'INCONCLUSIVE'),
            ('P8R3C_VENDOR_TO_APP_INGRESS', 'INCONCLUSIVE'),
            ('P8R3C_EXTERNAL_CALLBACK_QUIESCENCE', 'PROVEN'),
            ('P8R3C_INTEGRATION_READINESS', 'READY'),
            ('P8R3C_PROCESS_EXIT', '124'),
            ('P8R3C_RESULT', 'FAIL'),
        ]:
            with self.subTest(marker=key):
                mutated = valid_summary().replace(
                    f'{key}={EXPECTED[key]}', f'{key}={alternative}', 1)
                with self.assertRaises(ValueError):
                    VALIDATE(mutated)
        with self.assertRaises(ValueError):
            VALIDATE(valid_summary() + 'P8R3C_RESULT=PASS\n')
        with self.assertRaises(ValueError):
            VALIDATE(valid_summary().replace('P8R3C_STOP=RETURNED\n', ''))
        with self.assertRaises(ValueError):
            VALIDATE(valid_summary() + 'Authorization: Digest secret\n')

    def test_exact_vendor_hook_anchor_and_no_pointer_use(self):
        diff = PATCH.read_text(encoding='utf-8')
        self.assertIn('a/pjsip/src/pjsua2/endpoint.cpp', diff)
        self.assertIn('b/pjsip/src/pjsua2/endpoint.cpp', diff)
        self.assertEqual(diff.count('voip_phase8_r3c_after_lookup(call_id,'), 2)
        self.assertIn('voip_phase8_r3c_after_lookup(call_id, 1U)', diff)
        self.assertIn('voip_phase8_r3c_after_lookup(call_id, 2U)', diff)
        self.assertIn('voip_phase8_r3c_vendor_after_lookup != nullptr', diff)
        self.assertEqual(diff.count('Call::lookup'), 0)
        self.assertNotIn('call->delete', diff)
        self.assertNotIn('call->~', diff)
        self.assertEqual(diff.count('+++ b/'), 1)

    def test_production_hooks_lab_guard_only(self):
        source = APP.read_text(encoding='utf-8')
        self.assertEqual(source.count('voip_phase8_r3c_app_before_gate(getId(),'), 2)
        for kind in ('1U', '2U'):
            snippet = f'''#if defined(VOIP_PHASE8_R3C_LAB)
        voip_phase8_r3c_app_before_gate(getId(), {kind});
#endif
        auto callback_entry = context_->call_activity.enter();'''
            self.assertIn(snippet, source)
        self.assertEqual(source.count('call_.reset();'), 3)

    def test_live_requires_two_explicit_approvals(self):
        script = SCRIPT.read_text(encoding='utf-8')
        first = script.index("[[ \"${PHASE8_LIVE_SIP:-}\" == '1' ]]")
        second = script.index("[[ \"${PHASE8_R3C_CONFIRM:-}\" == '1002-to-1001' ]]")
        archive = script.index('git -C "$vendor_source" archive')
        traffic = script.index('timeout --signal=TERM')
        self.assertLess(first, second)
        self.assertLess(second, archive)
        self.assertLess(archive, traffic)
        self.assertIn('P8R3C_EXTERNAL_CALLBACK_QUIESCENCE',
                      (LAB / 'phase8_r3c_vendor_dispatch.cpp').read_text(encoding='utf-8'))
        self.assertNotIn('git -C "$vendor_source" apply', script)
        self.assertNotIn('rm -rf', script)
        self.assertIn('PKG_CONFIG_LIBDIR="$prefix/lib/pkgconfig"', script)
        self.assertIn('git -C "$vendor" apply --unidiff-zero --check', script)

    def test_live_runner_rejects_unapproved_execution(self):
        env = dict(os.environ)
        env.pop('PHASE8_LIVE_SIP', None)
        env.pop('PHASE8_R3C_CONFIRM', None)
        result = subprocess.run(['bash', str(SCRIPT)], env=env,
                                capture_output=True, text=True, timeout=5,
                                check=False)
        self.assertNotEqual(result.returncode, 0)
        self.assertIn('PHASE8_LIVE_SIP=1', result.stderr)
        env['PHASE8_LIVE_SIP'] = '1'
        result = subprocess.run(['bash', str(SCRIPT)], env=env,
                                capture_output=True, text=True, timeout=5,
                                check=False)
        self.assertNotEqual(result.returncode, 0)
        self.assertIn('PHASE8_R3C_CONFIRM=', result.stderr)

    def test_vendor_source_exact_match_if_operator_checkout_supplied(self):
        value = os.environ.get('PHASE8_R3C_VENDOR_SOURCE', '')
        if not value:
            self.skipTest('vendor checkout optional; no vendor tree in ordinary CI')
        vendor = Path(value)
        source = vendor / 'pjsip/src/pjsua2/endpoint.cpp'
        self.assertTrue(source.is_file())
        import hashlib
        self.assertEqual(hashlib.sha256(source.read_bytes()).hexdigest(),
                         '0bd796348c29272ceb98d49bd15574f02935bb28c4c268ea92fe7fdec4ed727c')
        result = subprocess.run(['git', '-C', str(vendor), 'apply', '--unidiff-zero', '--check', str(PATCH)],
                                capture_output=True, text=True, check=False)
        self.assertEqual(result.returncode, 0, 'vendor hook must apply to tag 2.17')


if __name__ == '__main__':
    unittest.main()
