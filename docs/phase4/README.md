# Phase 4 — SIP + Asterisk Lab

## Goal

Phase 4 introduces SIP at the protocol/lab level before the project integrates PJSIP into C++.
The first milestone is a reproducible local Asterisk environment with two SIP accounts that can
register and call each other across the LAN.

This phase deliberately starts with existing SIP clients. The project should prove that the server,
accounts, dialplan, signaling path, SDP negotiation, and RTP path work before a custom softphone is
added in Phase 5.

## Lab topology

```text
                       ThinkPad / Ubuntu
                 +-------------------------+
                 | Asterisk                |
                 | UDP SIP :5060           |
                 |                         |
                 | endpoint 1001           |
                 | endpoint 1002           |
                 +------------+------------+
                              |
                         Wi-Fi router
                       /              \
                      /                \
             SIP client 1001      SIP client 1002
             ThinkPad/desktop      Redmi 12 5G
```

The repository configuration uses `direct_media=no`, so Asterisk remains in the media path. That
makes later RTP capture and analysis easier to reason about.

## Repository files

```text
config/asterisk/pjsip.conf.example
config/asterisk/extensions.conf.example
scripts/verify_asterisk.sh
```

`pjsip.conf.example` defines:

```text
UDP transport on 0.0.0.0:5060
endpoint/auth/AoR for 1001
endpoint/auth/AoR for 1002
G.711 u-law only
voip-lab dialplan context
```

`extensions.conf.example` lets 1001 and 1002 dial each other's registered PJSIP contacts.

The committed passwords are example lab credentials, not production secrets:

```text
1001 / voip-lab-1001
1002 / voip-lab-1002
```

Change them before using the configuration on a network you do not fully control.

## 1. Install Asterisk

On Ubuntu:

```bash
sudo apt update
sudo apt install -y asterisk

asterisk -V
sudo systemctl enable --now asterisk
sudo systemctl status asterisk --no-pager
```

Before replacing any system configuration, save the current files:

```bash
sudo cp -a /etc/asterisk/pjsip.conf \
    /etc/asterisk/pjsip.conf.before-phase4

sudo cp -a /etc/asterisk/extensions.conf \
    /etc/asterisk/extensions.conf.before-phase4
```

## 2. Validate the repository samples

This check does not require a running Asterisk process:

```bash
./scripts/verify_asterisk.sh --samples
```

Expected final line:

```text
Asterisk sample configuration: PASS
```

## 3. Install the lab configuration

Copy the minimal lab files into Asterisk's configuration directory:

```bash
sudo install -o asterisk -g asterisk -m 0640 \
    config/asterisk/pjsip.conf.example \
    /etc/asterisk/pjsip.conf

sudo install -o asterisk -g asterisk -m 0640 \
    config/asterisk/extensions.conf.example \
    /etc/asterisk/extensions.conf
```

Restart Asterisk rather than relying only on a module reload. The lab file defines the PJSIP
transport, and transport changes are safest to apply with a full Asterisk restart.

```bash
sudo systemctl restart asterisk
sudo systemctl status asterisk --no-pager
```

Then verify the live configuration:

```bash
sudo ./scripts/verify_asterisk.sh --runtime
```

Useful manual CLI checks:

```bash
sudo asterisk -rx "pjsip show endpoints"
sudo asterisk -rx "pjsip show aors"
sudo asterisk -rx "pjsip show contacts"
sudo asterisk -rx "dialplan show voip-lab"
```

Before any client registers, the endpoints and AoRs should exist but the contact list can be empty.

## 4. Find the Asterisk server's LAN address

Use the ThinkPad's Wi-Fi/LAN IPv4 address as the SIP server address on both clients:

```bash
ip -brief -4 address
```

Do not use `127.0.0.1` from the Redmi. Loopback only refers to the device itself.

If UFW is enabled, the local lab needs SIP signaling and RTP to reach Asterisk. Check the current
firewall state before changing it:

```bash
sudo ufw status
```

For the default Asterisk UDP lab, the relevant traffic is SIP UDP/5060 and the Asterisk RTP UDP
range configured by the installed `rtp.conf` (commonly 10000-20000). Keep the lab on the local LAN;
do not expose these ports to the public Internet for this phase.

## 5. Configure two SIP clients

Client 1001:

```text
username/auth user: 1001
password:           voip-lab-1001
server/domain:      <ThinkPad LAN IPv4>
port:               5060
transport:          UDP
```

Client 1002:

```text
username/auth user: 1002
password:           voip-lab-1002
server/domain:      <ThinkPad LAN IPv4>
port:               5060
transport:          UDP
```

After both clients register:

```bash
sudo asterisk -rx "pjsip show contacts"
```

There should be a reachable contact for both 1001 and 1002.

## 6. First call test

Call 1002 from 1001, answer on 1002, verify two-way audio, then hang up. Repeat in the reverse
direction.

At the signaling level, Phase 4 should eventually capture and explain the important sequence:

```text
REGISTER
401 Unauthorized
REGISTER with Authorization
200 OK

INVITE
100 Trying
180 Ringing
200 OK
ACK
RTP
BYE
200 OK
```

Do not treat that sequence as a substitute for inspecting the actual packets. Later Phase 4 commits
will add Wireshark captures/evidence and a project-authored `docs/protocols/sip.md` based on the
observed messages.

## Phase boundary

Commit 1 is infrastructure only. It does not add PJSIP C++ code and does not claim that registration
or calling has already been demonstrated on the real LAN.

The expected progression is:

```text
Commit 1  Asterisk lab configuration and verification helper
Commit 2  registration experiment and captured evidence
Commit 3  call experiment and captured evidence
Commit 4  SIP/SDP protocol notes based on observations
Commit 5  Phase 4 verification/evidence and milestone preparation
```

Phase 5 will build the custom PJSIP CLI softphone on top of this known-good SIP lab.
