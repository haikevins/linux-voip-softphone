#!/usr/bin/env python3
"""Phase 8: bounded structural inventory for actual outgoing/incoming PJSUA2 source.

This audits reviewed offline teardown invariants and records unresolved risks; it
is not a race or external callback-quiescence proof. An exit code of zero means
only that the checked source structure remains recognizable and the audit could
be completed.
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
        context = _extract(clean, rf'\bstruct\s+{client}::CallbackContext\b[^{{;]*')
        account = _extract(clean, rf'\bclass\s+{client}::{account_class}\b[^{{;]*')
        call = _extract(clean, rf'\bclass\s+{client}::{call_class}\b[^{{;]*')
        reg = _extract(account, r'\bvoid\s+onRegState\s*\([^)]*\)\s*override')
        incoming = _extract(account, r'\bvoid\s+onIncomingCall\s*\([^)]*\)\s*override')
        state = _extract(call, r'\bvoid\s+onCallState\s*\([^)]*\)\s*override')
        media = _extract(call, r'\bvoid\s+onCallMediaState\s*\([^)]*\)\s*override')
        start = _extract(clean, rf'\bvoid\s+{client}::start\s*\(\s*\)')
        stop = _extract(clean, rf'\bvoid\s+{client}::stop\s*\(\s*\)\s*noexcept')
        destructor = _extract(clean, rf'\b{client}::~{client}\s*\(\s*\)')
        account_shutdown = _extract(
            clean, rf'\bbool\s+{client}::shutdown_account_safely\s*\([^)]*\)\s*noexcept')
        call_cleanup = _extract(
            clean, rf'\bbool\s+{client}::wait_for_call_cleanup\s*\([^)]*\)\s*noexcept')
    except ValueError as error:
        return {"checks": {"source_parseable": False}, "risks": ["STRUCTURE_UNRECOGNIZED"],
                "errors": [str(error)]}

    account_entry = 'context_->account_activity.enter()'
    call_entry = 'context_->call_activity.enter()'

    def owner_after_admission(body: str, entry: str) -> bool:
        entry_pos = body.find(entry)
        admitted_pos = body.find('.admitted()')
        owner_pos = body.find('context_->owner()')
        return 0 <= entry_pos < admitted_pos < owner_pos

    checks["callbacks_present"] = all([reg, incoming, state, media])
    checks["callback_context_owner_atomic"] = (
        f'std::atomic<{client} *> owner_' in context
        and 'owner_.load(std::memory_order_acquire)' in context
        and 'owner_.store(nullptr, std::memory_order_release)' in context
    )
    checks["callback_context_owns_gates"] = (
        'detail::CallbackActivityGate account_activity;' in context
        and 'detail::CallbackActivityGate call_activity;' in context
    )
    checks["account_uses_shared_callback_context"] = (
        'std::shared_ptr<CallbackContext> context_' in account
        and 'context_(std::move(context))' in account
    )
    checks["call_uses_shared_callback_context"] = (
        'std::shared_ptr<CallbackContext> context_' in call
        and 'context_(std::move(context))' in call
    )
    checks["callbacks_do_not_borrow_owner_reference"] = (
        'owner_(owner)' not in account
        and 'owner_(owner)' not in call
        and f'{client} &owner_' not in account
        and f'{client} &owner_' not in call
    )
    checks["registration_callback_gate"] = account_entry in reg and '.admitted()' in reg
    checks["incoming_callback_gate"] = account_entry in incoming and '.admitted()' in incoming
    checks["incoming_rejects_after_gate_close"] = (
        checks["incoming_callback_gate"]
        and 'pj::Call rejected_call' in incoming
        and 'PJSIP_SC_BUSY_HERE' in incoming
    )
    checks["state_callback_gate"] = call_entry in state and '.admitted()' in state
    checks["media_callback_gate"] = call_entry in media and '.admitted()' in media
    checks["owner_lookup_after_gate_admission"] = all((
        owner_after_admission(reg, account_entry),
        owner_after_admission(incoming, account_entry),
        owner_after_admission(state, call_entry),
        owner_after_admission(media, call_entry),
    ))
    checks["incoming_callback_mutex"] = 'owner->incoming_callback_mutex_' in incoming
    checks["state_callback_mutex"] = 'owner->call_callback_mutex_' in state
    checks["media_callback_mutex"] = 'owner->call_callback_mutex_' in media
    checks["polling_uses_real_call_slots"] = (
        'detail::wait_for_zero_call_slots(' in call_cleanup
        and 'pjsua_call_get_count()' in call_cleanup)
    checks["restart_requires_empty_callback_gates"] = (
        'callback_context_->account_activity.reset_if_empty()' in start
        and 'callback_context_->call_activity.reset_if_empty()' in start)
    checks["restart_rejects_retained_teardown_state"] = all(token in start for token in (
        'library_created_', 'endpoint_ != nullptr', 'account_ != nullptr', 'call_ != nullptr',
        'retains teardown state'))
    checks["destructor_invokes_stop"] = 'stop();' in destructor

    account_close = stop.find('callback_context_->account_activity.close()')
    account_pre_drain = stop.find(
        'callback_context_->account_activity.wait_for_empty_for(std::chrono::seconds{2})')
    hangup = stop.find('hangup()')
    disconnect = stop.find('wait_for_disconnection(std::chrono::seconds{2})')
    call_close = stop.find('callback_context_->call_activity.close()')
    call_drain = stop.find(
        'callback_context_->call_activity.wait_for_empty_for(std::chrono::seconds{2})')
    slot_wait = stop.find('wait_for_call_cleanup(std::chrono::seconds{2})')
    call_reset = stop.find('call_.reset()')
    account_shutdown_pos = stop.find('shutdown_account_safely()')
    account_post_drain = stop.find(
        'callback_context_->account_activity.wait_for_empty_for(std::chrono::seconds{2})',
        account_pre_drain + 1)
    account_reset = stop.find('account_.reset()')
    lib_destroy = stop.find('endpoint_->libDestroy()')

    ordered = [
        account_close, account_pre_drain, hangup, disconnect, call_close, call_drain,
        slot_wait, call_reset, account_shutdown_pos, account_post_drain, account_reset,
        lib_destroy,
    ]
    checks["shutdown_order"] = (
        all(position >= 0 for position in ordered)
        and ordered == sorted(ordered)
        and len(set(ordered)) == len(ordered)
    )
    checks["account_gate_drains_before_call_shutdown"] = (
        account_close >= 0 and account_close < account_pre_drain < hangup)
    checks["disconnect_precedes_call_gate_close"] = (
        disconnect >= 0 and disconnect < call_close < call_drain)
    checks["call_gate_drains_before_slot_wait"] = call_drain >= 0 and call_drain < slot_wait
    checks["slot_failure_vetoes_call_release"] = bool(re.search(
        r'if\s*\(\s*!wait_for_call_cleanup\(std::chrono::seconds\{2\}\)\s*\)\s*\{\s*return;',
        stop, re.DOTALL)) and slot_wait < call_reset
    checks["call_released_before_account_shutdown"] = (
        call_reset >= 0 and call_reset < account_shutdown_pos)
    checks["account_failure_vetoes_account_release"] = bool(re.search(
        r'if\s*\(\s*!shutdown_account_safely\(\)\s*\)\s*\{\s*return;',
        stop, re.DOTALL)) and account_shutdown_pos < account_reset
    checks["account_gate_drains_after_shutdown"] = (
        account_shutdown_pos >= 0 and account_shutdown_pos < account_post_drain < account_reset)
    checks["libdestroy_failure_retains_endpoint"] = bool(re.search(
        r'endpoint_->libDestroy\(\);\s*\}\s*catch\s*\(\.\.\.\)\s*\{[^}]*return;\s*\}'
        r'\s*endpoint_\.reset\(\);', stop, re.DOTALL))
    checks["destructor_fails_closed_after_stop"] = bool(re.search(
        r'stop\(\);\s*if\s*\(\s*library_created_\s*\|\|\s*'
        r'endpoint_\s*!=\s*nullptr\s*\|\|\s*account_\s*!=\s*nullptr\s*\|\|\s*'
        r'call_\s*!=\s*nullptr\s*\)\s*\{[^}]*std::terminate\(\);',
        destructor, re.DOTALL))
    terminate_pos = destructor.find('std::terminate()')
    detach_pos = destructor.find('callback_context_->detach_owner()')
    checks["destructor_detaches_callback_owner"] = (
        checks["destructor_fails_closed_after_stop"]
        and terminate_pos >= 0
        and detach_pos > terminate_pos
    )
    checks["non_forced_account_shutdown"] = (
        'parameter.force = false;' in account_shutdown
        and 'account_->shutdown2(parameter)' in account_shutdown
        and 'return false;' in account_shutdown)

    if not checks["registration_callback_gate"]:
        risks.append('REGISTRATION_CALLBACK_NO_TEARDOWN_GATE')
    if not checks["slot_failure_vetoes_call_release"]:
        risks.append('CALL_SLOT_WAIT_RESULT_DISCARDED')
    if not checks["account_failure_vetoes_account_release"]:
        risks.append('ACCOUNT_SHUTDOWN_RESULT_DISCARDED')
    if not all(checks[key] for key in (
            "registration_callback_gate", "incoming_callback_gate",
            "state_callback_gate", "media_callback_gate")):
        risks.append('NO_PRODUCTION_CALLBACK_ACTIVITY_GATE')

    # A destructor cannot return while retaining callback-owning PJSUA2 members.
    # If explicit stop() cannot prove teardown, terminate before C++ member
    # destruction can invoke Account::~Account() and force-delete the account.
    if not checks["destructor_fails_closed_after_stop"]:
        risks.append('ACCOUNT_DESTROYED_AFTER_POSSIBLE_SHUTDOWN_FAILURE')

    if not checks["callbacks_do_not_borrow_owner_reference"]:
        risks.append('CALLBACKS_BORROW_OWNER_BY_REFERENCE')
    if not all(checks[key] for key in (
            "callback_context_owner_atomic", "callback_context_owns_gates",
            "account_uses_shared_callback_context", "call_uses_shared_callback_context",
            "owner_lookup_after_gate_admission", "destructor_detaches_callback_owner")):
        # This is an offline owner-lifetime contract only. It prevents callbacks
        # that reach a still-live derived target from dereferencing a destroyed
        # client. It does not prove PJSUA2 will never invoke a destroyed target.
        risks.append('CALLBACK_OWNER_CONTEXT_UNSAFE')

    return {"checks": checks, "risks": sorted(set(risks)), "errors": []}


def audit_pair(outgoing: str, incoming: str) -> dict:
    clients = {
        name: audit_client(text, *CLIENTS[name])
        for name, text in (("outgoing", outgoing), ("incoming", incoming))
    }
    passed = all(all(item['checks'].values()) and not item['errors'] for item in clients.values())
    return {
        "baseline": "b8cca4c",
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
