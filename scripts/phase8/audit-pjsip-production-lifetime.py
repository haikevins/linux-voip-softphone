#!/usr/bin/env python3
"""Phase 8: bounded structural inventory for actual outgoing/incoming PJSUA2 source.

This audits known invariants and records unresolved risks; it is not a race or
callback-quiescence proof. An exit code of zero means only that the checked
source structure remains recognizable and the audit could be completed.
"""

from __future__ import annotations

import argparse
import hashlib
import json
from pathlib import Path
import re
import sys

CLIENTS = {
    "outgoing": ("SipOutgoingCallClient", "CallAccount", "TrackedCall"),
    "incoming": ("SipIncomingCallClient", "IncomingAccount", "TrackedIncomingCall"),
}


def _clean(source: str) -> str:
    """Remove comments without moving code positions/lines."""
    pattern = r'//[^\n]*|/\*[\s\S]*?\*/'
    return re.sub(pattern, lambda match: re.sub(r'[^\n]', ' ', match.group()), source)


def _body_after(text: str, match: re.Match[str]) -> str:
    opening = text.find('{', match.end())
    if opening == -1:
        raise ValueError("function/class body missing")
    depth = 0
    quote = None
    escaped = False
    for index in range(opening, len(text)):
        value = text[index]
        if quote:
            if escaped:
                escaped = False
            elif value == "\\":
                escaped = True
            elif value == quote:
                quote = None
            continue
        if value in ('"', "'"):
            quote = value
        elif value == '{':
            depth += 1
        elif value == '}':
            depth -= 1
            if depth == 0:
                return text[opening + 1:index]
    raise ValueError("unbalanced braces")


def _extract(text: str, pattern: str) -> str:
    match = re.search(pattern, text, re.MULTILINE | re.DOTALL)
    if match is None:
        raise ValueError(f"missing structural entry: {pattern}")
    return _body_after(text, match)


def audit_client(source: str, client: str, account_class: str, call_class: str) -> dict:
    clean = _clean(source)
    checks: dict[str, bool] = {}
    risks: list[str] = []
    try:
        account = _extract(clean, rf'\bclass\s+{client}::{account_class}\b[^{{;]*')
        call = _extract(clean, rf'\bclass\s+{client}::{call_class}\b[^{{;]*')
        reg = _extract(account, r'\bvoid\s+onRegState\s*\([^)]*\)\s*override')
        incoming = _extract(account, r'\bvoid\s+onIncomingCall\s*\([^)]*\)\s*override')
        state = _extract(call, r'\bvoid\s+onCallState\s*\([^)]*\)\s*override')
        media = _extract(call, r'\bvoid\s+onCallMediaState\s*\([^)]*\)\s*override')
        stop = _extract(clean, rf'\bvoid\s+{client}::stop\s*\(\s*\)\s*noexcept')
        account_shutdown = _extract(clean, rf'\bbool\s+{client}::shutdown_account_safely\s*\([^)]*\)\s*noexcept')
        call_cleanup = _extract(clean, rf'\bbool\s+{client}::wait_for_call_cleanup\s*\([^)]*\)\s*noexcept')
    except ValueError as error:
        return {"checks": {"source_parseable": False}, "risks": ["STRUCTURE_UNRECOGNIZED"],
                "errors": [str(error)]}

    checks["callbacks_present"] = all([reg, incoming, state, media])
    checks["incoming_callback_mutex"] = 'incoming_callback_mutex_' in incoming
    checks["state_callback_mutex"] = 'call_callback_mutex_' in state
    checks["media_callback_mutex"] = 'call_callback_mutex_' in media
    checks["callback_owner_reference"] = ('owner_(owner)' in account and 'owner_(owner)' in call)
    checks["polling_uses_real_call_slots"] = (
        'detail::wait_for_zero_call_slots(' in call_cleanup and
        'pjsua_call_get_count()' in call_cleanup)

    # These five tokens are deliberately searched *inside stop()*; a helper
    # elsewhere in the same file cannot give this function correct ordering.
    operations = [
        'wait_for_call_cleanup(std::chrono::seconds{2})',
        'call_.reset()',
        'shutdown_account_safely()',
        'account_.reset()',
        'endpoint_->libDestroy()',
    ]
    positions = [stop.find(operation) for operation in operations]
    checks["shutdown_order"] = (
        all(position >= 0 for position in positions) and
        positions == sorted(positions) and len(set(positions)) == len(positions))
    checks["bounded_wait_before_release"] = positions[0] >= 0 and positions[0] < positions[1]
    checks["non_forced_account_shutdown"] = (
        'parameter.force = false;' in account_shutdown and
        'account_->shutdown2(parameter)' in account_shutdown and
        'return false;' in account_shutdown)

    if 'owner_.incoming_callback_mutex_' not in reg and 'owner_.call_callback_mutex_' not in reg:
        risks.append('REGISTRATION_CALLBACK_NO_TEARDOWN_GATE')
    if 'static_cast<void>(wait_for_call_cleanup(' in stop:
        risks.append('CALL_SLOT_WAIT_RESULT_DISCARDED')
    if 'static_cast<void>(shutdown_account_safely())' in stop:
        risks.append('ACCOUNT_SHUTDOWN_RESULT_DISCARDED')
    if ('account_.reset()' in stop and 'return false;' in account_shutdown and
            'static_cast<void>(shutdown_account_safely())' in stop):
        risks.append('ACCOUNT_DESTROYED_AFTER_POSSIBLE_SHUTDOWN_FAILURE')
    if 'CallbackActivityGate' not in source:
        risks.append('NO_PRODUCTION_CALLBACK_ACTIVITY_GATE')
    if 'owner_(owner)' in account or 'owner_(owner)' in call:
        risks.append('CALLBACKS_BORROW_OWNER_BY_REFERENCE')

    return {"checks": checks, "risks": sorted(set(risks)), "errors": []}


def audit_pair(outgoing: str, incoming: str) -> dict:
    clients = {
        name: audit_client(text, *CLIENTS[name])
        for name, text in (("outgoing", outgoing), ("incoming", incoming))
    }
    passed = all(all(item['checks'].values()) and not item['errors'] for item in clients.values())
    return {
        "baseline": "f77d0b3",
        "kind": "STATIC_SOURCE_CONTRACT_ONLY",
        "source_contract": "PASS" if passed else "FAIL",
        "live_callback_quiescence": "INCONCLUSIVE",
        "clients": clients,
    }


def main(argv: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--source-root', type=Path,
                        default=Path(__file__).resolve().parents[2])
    parser.add_argument('--json', action='store_true')
    parser.add_argument('--require-live-proof', action='store_true',
                        help='Exit nonzero while real PJSUA2 callback quiescence is unverified')
    args = parser.parse_args(argv)
    try:
        sources = {}
        for name, (client, _, _) in CLIENTS.items():
            path = args.source_root / 'src' / 'pjsip' / f'{client}.cpp'
            sources[name] = path.read_text(encoding='utf-8')
        result = audit_pair(sources['outgoing'], sources['incoming'])
    except OSError as error:
        print(f'FAIL: cannot load production source: {error}', file=sys.stderr)
        return 2
    result['source_sha256'] = {
        name: hashlib.sha256(text.encode('utf-8')).hexdigest()
        for name, text in sources.items()
    }
    if args.json:
        print(json.dumps(result, indent=2, sort_keys=True))
    else:
        for name, item in result['clients'].items():
            for check, ok in item['checks'].items():
                print(f'{name}: {check}: {"PASS" if ok else "FAIL"}')
            for risk in item['risks']:
                print(f'{name}: OPEN RISK: {risk}')
        print('Source contract:', result['source_contract'])
        print('Live callback quiescence: INCONCLUSIVE (not tested)')
    if result['source_contract'] != 'PASS':
        return 1
    return 3 if args.require_live_proof else 0


if __name__ == '__main__':
    sys.exit(main())
