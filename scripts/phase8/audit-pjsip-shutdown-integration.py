#!/usr/bin/env python3
"""Static review of production PJSUA2 teardown hardening.

Zero means the reviewed explicit-stop result handling and local callback gates
are present in both real SIP clients and no external-quiescence proof is being
fabricated. It does NOT mean destruction is safe: destructor failure policy,
raw callback owner references, live callback quiescence, and actual PJ_EBUSY
recovery remain closure blockers.
"""

from __future__ import annotations

import argparse
import hashlib
import importlib.util
import json
from pathlib import Path
import sys

ROOT = Path(__file__).resolve().parents[2]
AUDIT = ROOT / 'scripts/phase8/audit-pjsip-production-lifetime.py'
spec = importlib.util.spec_from_file_location('phase8_existing_source_audit', AUDIT)
if spec is None or spec.loader is None:
    raise RuntimeError('existing production source audit cannot be imported')
lifetime = importlib.util.module_from_spec(spec)
sys.modules[spec.name] = lifetime
spec.loader.exec_module(lifetime)

EXPECTED_REMAINING_RISKS = frozenset({
    'ACCOUNT_DESTROYED_AFTER_POSSIBLE_SHUTDOWN_FAILURE',
    'CALLBACKS_BORROW_OWNER_BY_REFERENCE',
})

RESOLVED_EXPLICIT_STOP_RISKS = frozenset({
    'CALL_SLOT_WAIT_RESULT_DISCARDED',
    'ACCOUNT_SHUTDOWN_RESULT_DISCARDED',
    'REGISTRATION_CALLBACK_NO_TEARDOWN_GATE',
    'NO_PRODUCTION_CALLBACK_ACTIVITY_GATE',
})

PROOFS = (
    'CallSlotsDrained',
    'AccountShutdownSucceeded',
    'LocalCallbacksDrained',
    'ExternalCallbackIngressQuiescent',
)

# No trusted provider of the last proof exists. If a future change attempts to
# integrate the authorization header into either stop(), force explicit review.
PROHIBITED_CLIENT_TOKENS = (
    'PjsipShutdownAuthorization',
    'ShutdownProof::',
    'ShutdownReceipt::',
    'ExternalCallbackIngressQuiescent',
)


def evaluate(outgoing: str, incoming: str, authorization_header: str) -> dict:
    """Return reviewed offline source facts; never return READY for live PJSUA2."""
    existing = lifetime.audit_pair(outgoing, incoming)
    header_clean = lifetime._clean(authorization_header)
    try:
        declaration = lifetime._extract(header_clean, r'\benum\s+class\s+ShutdownProof\b[^\{;]*')
        header_proofs = {proof: declaration.count(proof) == 1 for proof in PROOFS}
        header_proofs['Count'] = declaration.count('Count') == 1
    except ValueError:
        header_proofs = {proof: False for proof in (*PROOFS, 'Count')}

    per_client = {}
    for name, source in (('outgoing', outgoing), ('incoming', incoming)):
        previous = existing['clients'][name]
        cleaned = lifetime._clean(source)
        risks = frozenset(previous['risks'])
        tokens_found = sorted(token for token in PROHIBITED_CLIENT_TOKENS if token in cleaned)
        checks = {
            'reviewed_explicit_stop_structure': (
                not previous['errors'] and all(previous['checks'].values())
            ),
            'explicit_stop_result_and_gate_risks_resolved': (
                not (RESOLVED_EXPLICIT_STOP_RISKS & risks)
            ),
            'two_known_closure_risks_remain': risks == EXPECTED_REMAINING_RISKS,
            'no_unreviewed_authorization_wiring': not tokens_found,
        }
        per_client[name] = {
            'checks': checks,
            'open_risks': sorted(risks),
            'unreviewed_tokens': tokens_found,
            'source_sha256': hashlib.sha256(source.encode('utf-8')).hexdigest(),
        }

    recognized = (
        existing['source_contract'] == 'PASS'
        and all(header_proofs.values())
        and all(all(item['checks'].values()) for item in per_client.values())
    )
    return {
        'audit_contract': 'PASS' if recognized else 'REVIEW_REQUIRED',
        'explicit_stop_hardening': 'INTEGRATED' if recognized else 'REVIEW_REQUIRED',
        'integration_readiness': 'BLOCKED' if recognized else 'REVIEW_REQUIRED',
        'destructor_failure_policy': 'UNRESOLVED',
        'live_external_quiescence': 'UNPROVEN',
        'actual_pj_ebusy_recovery': 'UNPROVEN',
        'authorization_header_proofs': header_proofs,
        'clients': per_client,
    }


def main(argv: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--source-root', type=Path, default=ROOT)
    parser.add_argument('--json', action='store_true')
    parser.add_argument('--require-ready', action='store_true',
                        help='Fail while live integration is blocked; exit 3 for known blockers')
    args = parser.parse_args(argv)
    base = args.source_root / 'src/pjsip'
    try:
        report = evaluate(
            (base / 'SipOutgoingCallClient.cpp').read_text(encoding='utf-8'),
            (base / 'SipIncomingCallClient.cpp').read_text(encoding='utf-8'),
            (base / 'PjsipShutdownAuthorization.hpp').read_text(encoding='utf-8'),
        )
    except OSError as error:
        print(f'ABORT: missing production source: {error}', file=sys.stderr)
        return 2

    if args.json:
        print(json.dumps(report, sort_keys=True, indent=2))
    else:
        for name, item in report['clients'].items():
            print(f'{name}: {len(item["open_risks"])} known closure risks OPEN')
            for key, good in item['checks'].items():
                print(f'  {key}: {"PASS" if good else "REVIEW REQUIRED"}')
        print('Explicit-stop teardown hardening:', report['explicit_stop_hardening'])
        print('Integration readiness:', report['integration_readiness'])
        print('Destructor failure policy:', report['destructor_failure_policy'])
        print('Live external callback quiescence:', report['live_external_quiescence'])
        print('Actual PJ_EBUSY recovery:', report['actual_pj_ebusy_recovery'])

    if report['audit_contract'] != 'PASS':
        return 2
    if args.require_ready:
        return 3
    return 0


if __name__ == '__main__':
    sys.exit(main())
