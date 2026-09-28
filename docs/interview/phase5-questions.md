# Phase 5 Interview Review — PJSIP CLI Softphone

Use the Phase 5 implementation, architecture note, verifier, and scrubbed evidence as the basis for these answers. Focus on lifecycle, ownership, asynchronous callbacks, verification boundaries, and the distinction between signaling success and real media support.

## Why is PJSIP support behind `ENABLE_PJSIP` instead of being mandatory for every build?

The repository still contains C++ core, Linux, IPC, and networking targets that do not require PJPROJECT. Keeping PJSIP optional allows the base project and earlier-phase regression tests to build without a local PJSIP installation.

The `pjsip-debug` preset explicitly enables the dependency for Phase 5 work.

Repository references:

```text
CMakeLists.txt
CMakePresets.json
cmake/Dependencies.cmake
```

## Why does the build use `pkg-config` for PJPROJECT?

The local PJPROJECT installation publishes `libpjproject.pc`. `pkg-config` provides the include paths, library paths, compile flags, and transitive link requirements needed by a consumer.

This is especially important for the project's static PJPROJECT build because the final link needs the complete static dependency set, not only one top-level library name.

The dependency verifier also performs a small PJSUA2 compile/link smoke test before the full project build.

## What is the difference between `SipEndpointConfig` and `SipAccountConfig`?

`SipEndpointConfig` describes endpoint/transport-level configuration such as signaling transport and local listen port.

`SipAccountConfig` describes SIP identity/registrar/authentication settings and maps those values into the PJSUA2 account configuration.

Keeping them separate prevents CLI parsing and PJSUA2 object construction from becoming one large unstructured configuration block.

## What is the common PJSUA2 startup lifecycle used by the CLIs?

At a high level the clients perform:

```text
Endpoint creation
libCreate
libInit
transportCreate
libStart
Account creation
wait for asynchronous registration/call callbacks
controlled shutdown
```

The exact configuration and timeout handling are documented in `docs/architecture/pjsip-cli.md`.

## Why does a command-line program need synchronization around PJSUA2 callbacks?

PJSUA2 reports registration and call changes asynchronously from library-managed callback execution. The main CLI flow cannot safely assume that callback-updated fields change synchronously on the same thread.

The project serializes access to shared state and uses bounded waiting so the CLI can react to callback results without data races or infinite waits.

## What does `onRegState` provide to the registration CLI?

The registration callback exposes the result of the SIP registration transaction and whether the account is active/registered. The CLI records the state, wakes its waiting control flow, and reports a concise result instead of storing raw SIP authentication traffic.

## How does the outgoing-call CLI work at a high level?

It:

1. initializes PJSUA2 and registers the account;
2. creates a `pj::Call`-derived object;
3. builds the destination URI from the validated CLI configuration;
4. places one call;
5. monitors call-state callbacks until confirmed, failed, disconnected, or timed out;
6. optionally holds the confirmed call for a bounded period;
7. hangs up and waits for disconnection;
8. tears down call, account, and endpoint state in a controlled order.

It is intentionally one-shot rather than an interactive production phone.

## How does the incoming-call CLI create the call object?

The account receives an incoming-call callback containing the PJSUA2 call ID. The project creates/tracks its call object for that ID, sends ringing, optionally delays the answer, sends `200 OK`, waits for `CONFIRMED`, holds for the requested interval, then hangs up and exits.

The call object must remain alive for the callback/lifecycle period and must be released only after the call has reached a safe terminal state.

## Why does teardown order matter?

PJSUA2 objects are related: calls belong to an account/library context, and accounts belong to the endpoint/library lifetime.

Destroying higher-level objects while active calls or callbacks are still in flight can trigger forced deletion or busy-state errors. The project therefore waits for call disconnection, releases the tracked call, shuts down the account without forcing deletion, waits for active call count to reach zero where needed, and then destroys the endpoint/library.

This is an object-lifetime problem as much as a SIP problem.

## Why is the SIP password read from an environment variable instead of a command-line argument?

Command-line arguments can be exposed through shell history and process inspection. The project therefore avoids a `--password` argument and reads the secret from a named environment variable.

The value is also excluded from source code, tests, evidence, and documentation output.

This does not make environment variables a universal production secret store; it is a safer interface for the current local CLI lab than putting the password directly in argv.

## Why are the three CLIs one-shot programs?

A small one-shot lifecycle keeps the Phase 5 objective measurable:

```text
configure -> register -> perform one operation -> clean teardown -> exit
```

It allows the project to isolate PJSUA2 object lifetime, callbacks, timeouts, and cleanup before adding an interactive UI, long-lived state management, or real audio-device routing.

## What does the process-level CLI contract test verify?

`PjsipCliContractIntegrationTest` executes the built programs as processes and checks their public CLI behavior, including help/argument/error contracts and password-environment expectations.

It does not require the live Asterisk server, so it can run repeatably as part of automated verification.

Live registration and call success remain separate manual integration evidence because they depend on the LAN, Asterisk, and another endpoint.

## Why are both PJSIP and non-PJSIP regression runs important?

Enabling PJSIP introduces a large external dependency and additional targets. The project verifies the PJSIP-enabled path while also rebuilding/running the normal configuration to ensure earlier C++/Linux/networking functionality still works when PJSIP is disabled.

This protects the optional-dependency boundary.

## Why does a confirmed SIP call not prove audible two-way audio in Phase 5?

A confirmed SIP dialog proves signaling/session establishment. The PJSUA2 call can also negotiate media parameters and show RTP-related state, but the Phase 5 clients deliberately use the null audio device.

Therefore Phase 5 does not prove that microphone samples are captured, routed into the call, received from the network, and played through a real output device.

That application-owned media path is the objective of Phase 6.

## What is the difference between the Phase 4 and Phase 5 responsibilities?

Phase 4 proves the server/lab and protocol behavior using existing SIP clients.

Phase 5 proves the project's own PJSUA2 signaling and call-lifecycle implementation against that already-known lab.

This separation reduces the number of unknowns when diagnosing failures and makes the project history easier to review.

## What evidence is intentionally not committed?

The project does not commit:

```text
SIP passwords
raw Digest Authorization headers
raw SIP logger traces containing authentication material
SDP keying material
```

Instead, `evidence/05-pjsip/` stores sanitized result/state summaries for automated verification, registration, outgoing calls, and incoming calls.
