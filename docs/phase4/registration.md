# Phase 4 — SIP Registration Lab

## Purpose

Prove that the Asterisk lab accepts real SIP registrations for both configured identities. The acceptance criterion is not merely that endpoint objects exist: Asterisk must have a live contact bound to AoR `1001` and another live contact bound to AoR `1002`.

Registration is intentionally verified before call establishment so reachability/authentication problems are separated from INVITE, SDP, RTP, and call-lifecycle problems.

## Prerequisites

The Asterisk runtime check must pass first:

```bash
sudo ./scripts/phase4/verify-asterisk.sh --runtime
```

Before clients register, `pjsip show contacts` can legitimately report no contacts. This lab adds two actual SIP user agents and verifies the resulting runtime bindings.

## Lab topology

The final Phase 4 lab uses two independently configured SIP clients:

```text
Redmi / Linphone / endpoint 1001
              |
           Wi-Fi/LAN
              |
              v
ThinkPad / Ubuntu / Asterisk :5060
              ^
              |
   local/LAN path as configured
              |
ThinkPad SIP client / endpoint 1002
```

The phone must use an address that reaches the ThinkPad on the LAN. A SIP client running on the same ThinkPad as Asterisk may use the local address appropriate to that configuration. Do not hard-code a transient LAN address into tests or documentation.

Inspect current IPv4 addresses with:

```bash
ip -brief -4 address
```

## Endpoint configuration

### Endpoint 1001

Configure the phone-side client with:

```text
SIP username:       1001
Authentication ID:  1001
Password:           <configured SIP password>
Server/domain:      <reachable Asterisk address>
Port:               5060
Transport:          UDP
```

### Endpoint 1002

Configure the ThinkPad-side SIP client with:

```text
SIP username:       1002
Authentication ID:  1002
Password:           <configured SIP password>
Server/domain:      <reachable Asterisk address>
Port:               5060
Transport:          UDP
```

Use the credential configured for the corresponding active Asterisk auth object. Password values are runtime configuration and must not be copied into documentation, command arguments, shell history, evidence, or committed traces.

## Verify registration

After both clients report that they are registered, run:

```bash
sudo ./scripts/phase4/verify-registration.sh --check
```

The verifier checks that:

- the Asterisk CLI is reachable;
- `res_pjsip.so` is loaded;
- endpoint and AoR objects `1001` and `1002` exist;
- AoR `1001` has a registered contact;
- AoR `1002` has a registered contact;
- both contacts appear in the global PJSIP contact table.

Expected final line:

```text
SIP registration verification: PASS
```

Useful manual inspection commands are:

```bash
sudo asterisk -rx "pjsip show contacts"
sudo asterisk -rx "pjsip show endpoint 1001"
sudo asterisk -rx "pjsip show endpoint 1002"
sudo asterisk -rx "pjsip show aor 1001"
sudo asterisk -rx "pjsip show aor 1002"
```

Exact addresses, source ports, contact hashes, status text, and RTT values are runtime-dependent and must not be hard-coded into tests or documentation.

## Capture evidence

Once `--check` passes, capture selected non-secret runtime state:

```bash
sudo ./scripts/phase4/verify-registration.sh --capture
```

The capture is written under:

```text
evidence/phase4/registration/
```

Review generated files before committing them. Contact URIs may contain local addresses and ephemeral ports even though SIP passwords are excluded.

## What registration proves

A live contact proves that the client reached the configured SIP transport, authenticated as the configured identity, and established a current contact binding for the AoR.

A typical Digest exchange is conceptually:

```text
REGISTER
401 Unauthorized
REGISTER with Digest response
200 OK
```

The initial `401 Unauthorized` is a challenge, not by itself a registration failure. Protocol semantics are documented centrally in `docs/protocols/sip.md`.

## Troubleshooting

If one endpoint has no contact, check the client/server configuration before changing unrelated code:

- the client has a network path to Asterisk;
- server/domain points to an address reachable from that client;
- transport is UDP and the server port is 5060;
- username and authentication ID match the intended endpoint;
- the supplied password matches the corresponding active Asterisk auth object without printing or recording it;
- the client is using a generic SIP account rather than a provider-specific account type.

Then inspect Asterisk directly:

```bash
sudo asterisk -rvvv
```

Useful console commands are:

```text
pjsip show contacts
pjsip show endpoint 1001
pjsip show endpoint 1002
```

Exit the console with `quit` after inspection.

## Completion criteria

The registration lab is complete when both identities are registered simultaneously, `--check` passes, sanitized evidence has been captured, and no credential or raw authentication-bearing trace is added to the repository.

## References

- `docs/phase4/README.md`
- `docs/architecture/sip-asterisk.md`
- `docs/protocols/sip.md`
- `evidence/phase4/registration/`
