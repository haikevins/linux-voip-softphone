# Phase 4 — SIP Registration Experiment

## Goal

Commit 2 proves that the Asterisk lab from Commit 1 accepts real SIP registrations for both
configured identities. The acceptance criterion is not merely that endpoint objects exist: Asterisk
must have a live contact bound to AoR `1001` and another live contact bound to AoR `1002`.

This commit still does not claim that a SIP call works. INVITE/SDP/RTP call-flow evidence belongs to
Commit 3.

## Prerequisite

The Commit 1 runtime check must already pass:

```bash
sudo ./scripts/verify_asterisk.sh --runtime
```

Before any client registers, `pjsip show contacts` can legitimately report no objects. Commit 2
changes that by registering two actual SIP user agents.

## Lab topology

Use two independently configured SIP clients:

```text
Redmi 12 5G / Linphone Android / endpoint 1001
                  |
               Wi-Fi
                  |
                  v
ThinkPad / Ubuntu / Asterisk UDP :5060
                  ^
                  |
          loopback 127.0.0.1
                  |
ThinkPad / baresip / endpoint 1002 / local SIP :5062
```

Endpoint `1001` reaches Asterisk over the LAN, while endpoint `1002` runs locally on the same
ThinkPad and reaches Asterisk over the loopback interface. Using two different SIP user agents also
provides a simple interoperability check instead of testing both endpoints with the same client
implementation.

Find the ThinkPad LAN IPv4 address used by the Redmi with:

```bash
ip -brief -4 address
```

Use an active Wi-Fi/Ethernet address, not `127.0.0.1`, as the SIP server address on the Redmi.

## Client 1001 — Linphone Android

Configure Linphone on the Redmi 12 5G with:

```text
SIP username:       1001
Authentication ID:  1001
Password:           voip-lab-1001
Server/domain:      <ThinkPad LAN IPv4>
Port:               5060
Transport:          UDP
```

## Client 1002 — baresip

Run baresip on the ThinkPad with its local SIP listener on `127.0.0.1:5062` and register to
Asterisk on `127.0.0.1:5060`.

The relevant baresip configuration is:

```text
sip_listen 127.0.0.1:5062
```

and the account is:

```text
<sip:1002@127.0.0.1:5060;transport=udp>;auth_user=1002;auth_pass=voip-lab-1002;regint=3600
```

Keep baresip running while registration evidence is captured.

These are intentionally simple lab credentials committed in the repository. Do not reuse them for a
public or production SIP service.

## Verify registration

After both clients report that they are registered, run:

```bash
sudo ./scripts/verify_sip_registration.sh --check
```

The verifier checks all of the following:

- Asterisk CLI is reachable.
- `res_pjsip.so` is loaded.
- endpoint and AoR objects `1001` and `1002` exist.
- AoR `1001` has a registered contact.
- AoR `1002` has a registered contact.
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

A successful contact line should identify the AoR and the client contact URI. Exact addresses,
source ports, hashes, status text, and RTT values are runtime-dependent and must not be hard-coded
into tests or documentation.

## Capture evidence

Once `--check` passes, capture the live non-secret state:

```bash
sudo ./scripts/verify_sip_registration.sh --capture
```

The command first re-runs the verification and only writes evidence if both registrations are
present. It creates:

```text
evidence/04-sip/registration/
├── README.md
├── aors.txt
├── asterisk-version.txt
├── contacts.txt
├── endpoints.txt
└── registration-summary.txt
```

The generated evidence contains Asterisk runtime state and contact addresses, but not the configured
SIP passwords. Review the files before committing them anyway, because contact URIs contain local
network addresses and ephemeral client ports.

The collector strips trailing whitespace from generated text so `git diff --check` remains a valid
commit gate.

## What registration demonstrates

A registered contact proves that the client reached the Asterisk SIP transport, authenticated as the
configured endpoint, and established a current contact binding for the AoR. In a typical digest-auth
exchange the observable signaling is:

```text
REGISTER
401 Unauthorized
REGISTER with Authorization
200 OK
```

Commit 2 records the resulting server state. Packet-level inspection of the exact SIP messages is
reserved for the later protocol-evidence commit; do not infer packet fields solely from the summary
above.

## Troubleshooting

If one endpoint has no contact, check the client configuration before changing the server:

- the phone and ThinkPad are on a network path that permits client-to-client LAN traffic;
- server/domain points to the ThinkPad LAN IPv4 address;
- transport is UDP and server port is 5060;
- username and authentication ID match the endpoint number;
- the password matches `config/asterisk/pjsip.conf.example`;
- the client is using a generic SIP account rather than a provider-specific account type.

Then inspect Asterisk directly:

```bash
sudo asterisk -rvvv
```

From the Asterisk console, the following commands are useful while a client retries registration:

```text
pjsip show contacts
pjsip show endpoint 1001
pjsip show endpoint 1002
```

Exit the console with `quit` after inspection.

## Commit boundary

Commit 2 is complete when both clients are registered simultaneously, `--check` passes, `--capture`
has produced evidence from the real lab, `git diff --check` is clean, and those artifacts are
committed together with this verifier and experiment guide.
