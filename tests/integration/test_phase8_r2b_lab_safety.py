#!/usr/bin/env python3
"""Offline opt-in guard/source-scope checks; NEVER makes SIP connections."""

from __future__ import annotations

import os
from pathlib import Path
import subprocess
import unittest

ROOT = Path(__file__).resolve().parents[2]
SCRIPT = ROOT / 'scripts/phase8/run-production-busy-recovery-lab.sh'


class Phase8R2BLabSafetyTests(unittest.TestCase):
    def test_no_operator_opt_in_denies_before_build_or_network(self):
        env = os.environ.copy()
        for name in ('PHASE8_LIVE_SIP', 'PHASE8_R2B_CONFIRM', 'VOIP_SIP_PASSWORD'):
            env.pop(name, None)
        result = subprocess.run(
            ['bash', str(SCRIPT)],
            cwd=ROOT,
            env=env,
            stdin=subprocess.DEVNULL,
            capture_output=True,
            text=True,
            check=False,
            timeout=10,
        )
        self.assertNotEqual(result.returncode, 0)
        self.assertIn('explicit live SIP approval required', result.stderr)

    def test_second_consent_is_required(self):
        env = os.environ.copy()
        env['PHASE8_LIVE_SIP'] = '1'
        env.pop('PHASE8_R2B_CONFIRM', None)
        env.pop('VOIP_SIP_PASSWORD', None)
        result = subprocess.run(
            ['bash', str(SCRIPT)],
            cwd=ROOT,
            env=env,
            stdin=subprocess.DEVNULL,
            capture_output=True,
            text=True,
            check=False,
            timeout=10,
        )
        self.assertNotEqual(result.returncode, 0)
        self.assertIn('explicit active-call shutdown approval required', result.stderr)

    def test_compile_time_gate_and_production_stop_order(self):
        root_cmake = (ROOT / 'CMakeLists.txt').read_text(encoding='utf-8')
        src_cmake = (ROOT / 'src/CMakeLists.txt').read_text(encoding='utf-8')
        header = (ROOT / 'src/pjsip/SipOutgoingCallClient.hpp').read_text(encoding='utf-8')
        impl = (ROOT / 'src/pjsip/SipOutgoingCallClient.cpp').read_text(encoding='utf-8')

        self.assertIn('ENABLE_PHASE8_R2B_LAB "Build the opt-in Phase 8', root_cmake)
        self.assertIn('recovery lab" OFF)', root_cmake)
        self.assertIn('if(ENABLE_PHASE8_R2B_LAB)', src_cmake)
        self.assertIn('target_compile_definitions(voip_pjsip PRIVATE VOIP_PHASE8_R2B_LAB=1)', src_cmake)
        self.assertIn('#if defined(VOIP_PHASE8_R2B_LAB)', header)
        self.assertIn('#if defined(VOIP_PHASE8_R2B_LAB)', impl)
        self.assertIn('error_status != PJ_EBUSY', impl)

        start = impl.index('void SipOutgoingCallClient::stop() noexcept')
        end = impl.index('RegistrationResult\nSipOutgoingCallClient::wait_for_registration', start)
        stop = impl[start:end]
        self.assertLess(stop.index('wait_for_call_cleanup(std::chrono::seconds{2})'),
                        stop.index('if (!shutdown_account_safely())'))
        self.assertNotIn('lab_try_shutdown_while_call_active', stop)


if __name__ == '__main__':
    unittest.main()
