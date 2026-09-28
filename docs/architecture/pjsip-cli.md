# Phase 5 Architecture — PJSIP CLI Softphone

## Status

Completed — milestone `v0.6-pjsip-cli`.

## Purpose

Phase 5 introduces a narrow PJSUA2 integration layer that owns SIP endpoint/account lifecycle and
exposes three testable command-line programs. The design is intentionally synchronous at the CLI
boundary while PJSUA2 performs network work and invokes state callbacks from its worker context.

The production components are:

```text
src/pjsip/
├── SipEndpointConfig.{hpp,cpp}
├── SipAccountConfig.{hpp,cpp}
├── SipRegistrationCli.{hpp,cpp}
├── SipRegistrationClient.{hpp,cpp}
├── SipOutgoingCallCli.{hpp,cpp}
├── SipOutgoingCallClient.{hpp,cpp}
├── SipIncomingCallCli.{hpp,cpp}
└── SipIncomingCallClient.{hpp,cpp}

src/app/
├── sip_register_main.cpp
├── sip_call_main.cpp
└── sip_listen_main.cpp
```

They are compiled only when `ENABLE_PJSIP=ON`. The `voip_pjsip` library links to the imported
`PJSIP::PJSIP` dependency target, and the three executables link to `voip::pjsip`.

## Scope

- Integrate PJPROJECT/PJSUA2 behind an optional build boundary.
- Provide one-shot CLIs for registration, outgoing calls, and incoming calls.
- Make callback synchronization, timeouts, account/call ownership, and teardown order explicit.
- Verify signaling behavior without claiming Phase 6 two-way audio support.

## Architecture model

### Layering

```text
command line
    |
    v
Sip*Cli parser/options
    |
    +----> SipEndpointConfig
    |          transport + local listen port
    |
    +----> SipAccountConfig
               username + password + server + realm + registrar
    |
    v
Sip*Client
    |
    +----> pj::Endpoint
    +----> pj::Account subclass
    +----> pj::Call subclass (call clients only)
    |
    v
PJSUA2 / PJSIP
    |
    v
Asterisk
```

The parser/configuration layer contains deterministic argument validation. The client layer owns
PJSUA2 objects and translates asynchronous callback state into results that the synchronous CLI can
wait on.

### Configuration objects

#### `SipEndpointConfig`

`SipEndpointConfig` contains:

```text
SipTransport transport   UDP by default, TCP supported
uint16_t listen_port     0 by default
```

It maps directly to `pj::TransportConfig`. Port `0` delegates local port selection to PJSIP, which
is useful when a fixed development port is already occupied.

#### `SipAccountConfig`

`SipAccountConfig` contains:

```text
username
password
server
server_port = 5060
realm = "*"
transport = UDP
register_on_add = true
```

It validates simple URI components and produces:

```text
identity URI:   sip:<username>@<server>
registrar URI:  sip:<server>:<port>;transport=<udp|tcp>
```

`to_pjsua2_account_config()` installs the Digest credential and maps `register_on_add` to
PJSUA2's registration configuration.

The password is not accepted as a CLI argument. The executable reads it from the environment only
after argument parsing succeeds, then assigns it to `SipAccountConfig` in memory.

## Runtime and lifecycle

### Common endpoint startup sequence

All three clients follow the same core PJSUA2 initialization sequence:

```text
construct client
    |
    v
pj::Endpoint::libCreate()
    |
    v
pj::Endpoint::libInit()
    |
    v
transportCreate(...)
    |
    v
pj::Endpoint::libStart()
    |
    v
create pj::Account subclass
    |
    v
Account::create(...)
    |
    v
wait for callback-driven registration result
```

The transport ID returned by `transportCreate()` is written into the account configuration so the
account uses the endpoint transport created for that client.

The outgoing and incoming call clients call `audDevManager().setNullDev()` after `libStart()`. This
keeps Phase 5 focused on signaling and lifecycle rather than connecting sound devices.

### Registration client

`SipRegistrationClient` owns:

```text
pj::Endpoint
RegistrationAccount : pj::Account
registration mutex + condition_variable
RegistrationResult
```

`RegistrationAccount::onRegState()` obtains `pj::AccountInfo` and forwards it to the owner. The
owner records the status code/reason and whether registration is active, then signals the waiting
CLI thread.

The high-level flow is:

```text
start()
  |
  v
REGISTER transaction
  |
  v
onRegState()
  |
  v
RegistrationResult
  |
  v
wait_for_registration() wakes
```

This client is intentionally a registration verifier, not a persistent user agent.

### Outgoing call client

`SipOutgoingCallClient` adds two PJSUA2 subclasses:

```text
CallAccount : pj::Account
TrackedCall : pj::Call
```

`CallAccount::onRegState()` feeds registration state into the client. `TrackedCall::onCallState()`
reads `pj::CallInfo` and feeds call state into the client.

The CLI flow is:

```text
start endpoint/account
        |
        v
wait for registration
        |
        v
make_call(destination_uri)
        |
        v
pj::Call::makeCall()
        |
        v
onCallState() callbacks
        |
        +---- early/failure state
        |
        +---- CONFIRMED -> connected result
        |
        v
optional hold interval
        |
        v
hangup()
        |
        v
wait_for_disconnection()
        |
        v
teardown
```

The CLI constructs the destination URI as:

```text
sip:<target>@<server>:<server_port>;transport=<udp|tcp>
```

The target parser intentionally rejects a preformatted URI. This keeps ownership of URI
construction inside the project configuration layer.

### Incoming call client

`SipIncomingCallClient` uses:

```text
IncomingAccount : pj::Account
TrackedIncomingCall : pj::Call
```

`IncomingAccount::onIncomingCall()` receives the PJSUA2 call ID. The client creates
`TrackedIncomingCall(account, call_id)` so later state callbacks operate on the same call slot.

The high-level flow is:

```text
start endpoint/account
        |
        v
wait for registration
        |
        v
onIncomingCall(call_id)
        |
        v
create TrackedIncomingCall
        |
        v
send 180 Ringing
        |
        v
wait_for_incoming_call() wakes
        |
        v
optional answer delay
        |
        v
answer(200 OK)
        |
        v
onCallState() -> CONFIRMED
        |
        v
optional hold interval
        |
        v
hangup + disconnect + teardown
```

The current executable handles exactly one incoming call and then exits. Multi-call handling and a
long-running UI event model are intentionally outside this CLI milestone.

### Callback synchronization

PJSUA2 callbacks are asynchronous relative to the CLI thread. The clients therefore protect shared
results with mutexes and wake waiters with condition variables.

Conceptually:

```text
PJSUA2 worker/callback context          CLI/main thread
-----------------------------          ---------------
onRegState()                   ----->  wait_for_registration()
onCallState()                  ----->  wait_for_call()
onIncomingCall()               ----->  wait_for_incoming_call()
disconnected state             ----->  wait_for_disconnection()
```

Callback bodies catch `pj::Error`, standard exceptions, and unknown exceptions before the error can
escape through PJSUA2 callback boundaries. The error text is converted into client result state
where applicable.

The outgoing and incoming clients also serialize callback access around the tracked `pj::Call`
object during teardown. This prevents the main thread from destroying the C++ call wrapper while a
call-state callback is still executing.

### Teardown ordering

Call teardown is more subtle than observing the `DISCONNECTED` state. PJSUA can report the state
callback before the internal call slot has completed transaction cleanup.

The call clients therefore use this ordering:

```text
request hangup if needed
        |
        v
wait for DISCONNECTED result
        |
        v
serialize with call callbacks
        |
        v
destroy TrackedCall C++ wrapper
        |
        v
wait until pjsua_call_get_count() == 0
        |
        v
Account::shutdown2(force=false)
        |
        v
destroy account
        |
        v
Endpoint::libDestroy()
```

Waiting for `pjsua_call_get_count() == 0` is important because the callback-visible disconnected
state does not by itself prove that the PJSUA call slot is reusable. The Phase 5 live tests exposed
this distinction: destroying the account too early produced an active-call warning or `PJ_EBUSY`.
The final lifecycle waits for PJSUA cleanup before shutting down the account.

### Failure and timeout model

The CLI programs use two broad exit categories:

```text
0   requested verification succeeded
1   PJSIP/network/protocol result did not complete successfully
2   local CLI/configuration/password-environment error
```

Registration/call timeouts produce explicit result paths rather than indefinite blocking. When an
outgoing or answered incoming call times out, the executable requests hangup and waits briefly for
disconnection before exiting.

## Verification strategy

### Test strategy

Phase 5 has three verification layers.

#### Unit/configuration tests

GoogleTest covers:

```text
SipAccountConfig validation and PJSUA2 mapping
SipEndpointConfig transport/port mapping
registration CLI parsing
outgoing call CLI parsing and destination URI construction
incoming call CLI parsing
```

#### Process-level CLI contract test

`tests/integration/test_pjsip_cli_contract.sh` runs the built executables and checks their public
command-line contract without depending on an Asterisk server. It verifies help behavior and the
password-environment boundary for all three CLIs.

#### Live Asterisk tests

Manual lab verification covers behavior that requires real SIP peers:

```text
1002 registration -> Asterisk 200 OK
1002 outgoing call -> 1001 -> CONFIRMED -> clean disconnect
1001 incoming call -> 1002 -> CONFIRMED -> clean disconnect
```

Live results should be recorded as high-level evidence only. Raw SIP logs are intentionally excluded
because authentication and media-security values may appear in signaling traces.

## Design rationale

### Why this is still a CLI architecture

The three clients are deliberately small, one-operation executables. That makes PJSUA2 ownership,
callback synchronization, timeout behavior, and shutdown order observable before the project adds a
Qt/QML UI or a long-running concurrent call controller.

Phase 7 can place a UI above a stable signaling abstraction, and Phase 8 can introduce broader
threading/event-dispatch policy without having to learn basic PJSUA2 lifetime rules at the same time.
Phase 6 comes first and adds the media path while preserving the signaling lifecycle established
here.

## Phase boundary

Phase 5 proves project-owned SIP registration and call lifecycle control through PJSUA2. Media is intentionally kept on the null-audio path for lifecycle verification; audible two-way RTP/audio-device work belongs to Phase 6.

## References

- `docs/phase5/README.md`
- `docs/phase5/dependencies.md`
- `docs/interview/phase5-questions.md`
- `src/pjsip/SipRegistrationClient.hpp`
- `src/pjsip/SipOutgoingCallClient.hpp`
- `src/pjsip/SipIncomingCallClient.hpp`
- `scripts/verify_phase5.sh`
- `tests/integration/test_pjsip_cli_contract.sh`
