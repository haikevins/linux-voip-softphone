# Phase 4 SIP Registration Evidence

This directory records live Asterisk state for the Phase 4 registration experiment.

Do not fabricate or hand-edit successful registration output. Generate the runtime artifacts only
after SIP clients `1001` and `1002` are simultaneously registered:

```bash
sudo ./scripts/verify_sip_registration.sh --capture
```

A successful capture adds:

```text
asterisk-version.txt
endpoints.txt
aors.txt
contacts.txt
registration-summary.txt
```

The verifier refuses to capture a successful evidence set if either endpoint lacks a registered
contact. Generated text is normalized to remove trailing whitespace before commit.

Contact URIs can contain private LAN addresses and ephemeral ports. Review the generated files before
publishing the repository. SIP passwords are not collected by this script.
