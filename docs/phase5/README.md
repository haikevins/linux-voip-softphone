# Phase 5 — PJSIP CLI Softphone

## Goal

Phase 5 replaces the temporary command-line SIP endpoint used in the Asterisk lab with project-owned
C++20/PJSUA2 clients. The phase covers PJSIP dependency integration, SIP account configuration,
registration, one-shot outgoing calls, one-shot incoming calls, lifecycle cleanup, and repeatable
automated verification.

Media is deliberately not an application feature in this phase. The call clients select PJSUA2's
null audio device so SIP call signaling can be exercised without designing capture, playback,
RTP/media routing, or device selection. Those topics belong to Phase 6.

## Build prerequisites

The project expects PJPROJECT/PJSIP 2.17 or newer to expose the `libpjproject` pkg-config module.
The Phase 5 workstation installation uses:

```text
source:  ~/.local/src/pjproject-2.17
prefix:  ~/.local/pjproject-2.17
module:  ~/.local/pjproject-2.17/lib/pkgconfig/libpjproject.pc
```

Make the pkg-config metadata visible before configuring:

```bash
export PKG_CONFIG_PATH="$HOME/.local/pjproject-2.17/lib/pkgconfig${PKG_CONFIG_PATH:+:$PKG_CONFIG_PATH}"
```

Verify the dependency and build the PJSIP-enabled configuration:

```bash
./scripts/verify_pjsip_dependency.sh
cmake --preset pjsip-debug
cmake --build --preset pjsip-debug -j
```

The dedicated preset sets `ENABLE_PJSIP=ON`. Normal non-PJSIP builds remain supported.

## Produced CLI programs

The PJSIP-enabled build produces three small executables under `build/pjsip-debug/src/`:

```text
voip-sip-register   register one SIP account and report the result
voip-sip-call       register, place one outgoing call, hang up, and exit
voip-sip-listen     register, handle one incoming call, hang up, and exit
```

All three intentionally read the SIP password from an environment variable rather than a command
line argument. The default variable is `VOIP_SIP_PASSWORD`, and `--password-env` can select another
name.

Do not put the password in shell history, source files, test fixtures, evidence files, or committed
SIP traces.

## Registration CLI

Usage:

```text
voip-sip-register --username USER --server HOST [options]
```

Important options:

```text
--server-port PORT     registrar port, default 5060
--listen-port PORT     local SIP port, default 0 (PJSIP chooses)
--transport udp|tcp    signaling transport, default udp
--realm REALM          Digest realm, default *
--timeout-ms MS        registration timeout, default 10000
--password-env NAME    password environment variable
```

Example for the Asterisk lab account `1002`:

```bash
export VOIP_SIP_PASSWORD='<configured password for auth1002>'

./build/pjsip-debug/src/voip-sip-register \
    --username 1002 \
    --server 192.168.1.13 \
    --server-port 5060 \
    --listen-port 0 \
    --transport udp \
    --timeout-ms 10000

unset VOIP_SIP_PASSWORD
```

A successful registration ends with output similar to:

```text
[REGISTER] status=200 active=yes reason="OK"
```

The registration executable is intentionally one-shot. It validates that the account can register,
then lets the client shut down cleanly rather than acting as a long-running phone.

## Outgoing call CLI

Usage:

```text
voip-sip-call --username USER --server HOST --target USER [options]
```

The `--target` value is a SIP user or extension, not a preformatted SIP URI. The CLI constructs the
destination from the target, server, server port, and selected transport.

Additional call options:

```text
--registration-timeout-ms MS   registration timeout, default 10000
--call-timeout-ms MS           wait for answer/failure, default 30000
--hold-ms MS                   connected hold duration before BYE, default 2000
```

Example: account `1002` on the ThinkPad calls Linphone extension `1001` through Asterisk:

```bash
export VOIP_SIP_PASSWORD='<configured password for auth1002>'

./build/pjsip-debug/src/voip-sip-call \
    --username 1002 \
    --server 192.168.1.13 \
    --server-port 5060 \
    --listen-port 5062 \
    --transport udp \
    --target 1001 \
    --registration-timeout-ms 10000 \
    --call-timeout-ms 30000 \
    --hold-ms 3000

unset VOIP_SIP_PASSWORD
```

The successful live Phase 5 flow is:

```text
REGISTER -> 200 OK
INVITE -> ringing/answer through Asterisk
call state -> CONFIRMED
hold for the requested interval
BYE / disconnect
clean PJSUA2 teardown
```

A successful run includes:

```text
[REGISTER] status=200 active=yes reason="OK"
[CALL] state=CONFIRMED status=200 connected=yes disconnected=no
[CALL] disconnected=yes
```

## Incoming call CLI

Usage:

```text
voip-sip-listen --username USER --server HOST [options]
```

Additional incoming-call options:

```text
--registration-timeout-ms MS   registration timeout, default 10000
--incoming-timeout-ms MS       wait for an INVITE, default 30000
--call-timeout-ms MS           wait for CONFIRMED after answer, default 10000
--answer-delay-ms MS           delay before automatic 200 OK, default 0
--hold-ms MS                   connected hold duration before hangup, default 2000
```

Example: register account `1002`, then call extension `1002` from Linphone account `1001`:

```bash
export VOIP_SIP_PASSWORD='<configured password for auth1002>'

./build/pjsip-debug/src/voip-sip-listen \
    --username 1002 \
    --server 192.168.1.13 \
    --server-port 5060 \
    --listen-port 5062 \
    --transport udp \
    --registration-timeout-ms 10000 \
    --incoming-timeout-ms 30000 \
    --call-timeout-ms 10000 \
    --answer-delay-ms 500 \
    --hold-ms 3000

unset VOIP_SIP_PASSWORD
```

The listener handles one incoming call. It creates a tracked `pj::Call` from the PJSUA2 call ID,
sends ringing, auto-answers with `200 OK`, waits for `CONFIRMED`, holds the call, hangs up, and exits.

Expected high-level output:

```text
[REGISTER] status=200 active=yes reason="OK"
[INCOMING] state=INCOMING ... received=yes ...
[INCOMING] state=CONFIRMED status=200 received=yes connected=yes disconnected=no ...
[INCOMING] disconnected=yes
```

## Asterisk lab workflow

The Phase 5 live topology is:

```text
Linphone / extension 1001
          |
          | SIP
          v
Asterisk 20 / UDP 5060
          |
          | SIP
          v
PJSUA2 CLI / extension 1002
```

Before a test, ensure an older endpoint is not holding the chosen local port. For example:

```bash
sudo ss -lunp | grep ':5062' || true
pgrep -af 'baresip|voip-sip-register|voip-sip-call|voip-sip-listen' || true
```

Using `--listen-port 0` is also valid and lets PJSIP select an available local port.

The account password should match the currently configured Asterisk auth object. Do not assume a
password value from documentation; use the lab configuration as the source of truth.

## Automated verification

Commit 6 adds the Phase 5 verifier:

```bash
./scripts/verify_phase5.sh
```

It runs these gates:

```text
git diff --check
clang-format --dry-run --Werror
PJSIP dependency compile/link smoke test
PJSIP debug build
PJSIP-labeled tests
full PJSIP regression
ASan + UBSan with ENABLE_PJSIP=ON
clang-tidy with ENABLE_PJSIP=ON
non-PJSIP debug regression
```

The integration test `PjsipCliContractIntegrationTest` exercises the three executable interfaces at
the process boundary without requiring a live Asterisk server. Live registration and call-flow
verification remains manual because it depends on the lab network/endpoints and must not capture SIP
credentials into repository artifacts.

## Security and evidence rules

Keep evidence at the result/state level, for example:

```text
registration status code
registered yes/no
call state
connected/disconnected result
selected transport
verification test totals
```

Do not commit raw SIP logger output. SIP traces may contain Digest `Authorization` values and media
security attributes. Likewise, never commit `VOIP_SIP_PASSWORD` or shell output that prints it.

## Phase boundary

Phase 5 proves signaling and lifecycle behavior. It does not claim a finished media path.

```text
Phase 5
  PJSIP/PJSUA2 dependency
  SIP account/transport configuration
  REGISTER
  outgoing INVITE lifecycle
  incoming INVITE lifecycle
  deterministic teardown
  CLI/integration verification

Phase 6
  RTP/media behavior
  audio capture/playback
  device selection
  PJSUA2 media callbacks/connections
  codec/media validation
  one-way/no-audio diagnosis
```

The Phase 5 call clients use the PJSUA2 null audio device specifically to keep that boundary clear.

For implementation details and callback/lifetime design, see
`docs/architecture/pjsip-cli.md`. Dependency provisioning is documented in
`docs/phase5/dependencies.md`.
