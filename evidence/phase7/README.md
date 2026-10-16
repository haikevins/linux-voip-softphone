# Phase 7 Evidence — Qt/QML GUI

## Status

Completed. Phase 7 established the Qt/QML GUI and the `v0.8-qt-gui` milestone.

## Verification scope

Evidence covers the Phase 7 automated gate, Qt-only and Qt+PJSIP build/test integration, the offscreen GUI launch smoke test, live GUI registration, outgoing and incoming call control, connected-call duration, diagnostics, nonzero RTP send/receive activity, and operator-confirmed clear two-way audio.

## Environment

The recorded live GUI acceptance used the existing local Asterisk/PJPROJECT 2.17 lab with GUI account `1002` and peer `1001`. The accepted runtime source revision was `090c278`. The closure commit adds evidence, documentation, metadata, and structure checks rather than changing the accepted GUI call path. Local addresses, audio devices, timing, and media measurements are environment-specific observations rather than portable defaults.

## Automated verification

The Phase 7 verifier is:

```bash
export PKG_CONFIG_PATH="$HOME/.local/pjproject-2.17/lib/pkgconfig${PKG_CONFIG_PATH:+:$PKG_CONFIG_PATH}"
./scripts/phase7/verify.sh
```

The accepted run completed the inherited Phase 6 quality/regression gate, Qt-only and Qt+PJSIP builds, Phase 7-labeled controller/ViewModel and GUI-smoke tests, documentation verification, and evidence verification. The final Phase 7-labeled suite reported 11/11 passing tests. See `automated-verification.txt` for the concise acceptance summary.

## Manual verification

The operator registered the GUI account successfully, placed an outgoing call to peer `1001`, received and answered an incoming call from `1001`, observed the expected Ready/Ringing/Connected UI states, verified call duration and diagnostics, and confirmed clear intelligible speech in both directions for both call directions. `manual-verification.txt` records the accepted checklist.

Runtime PJSUA2 observations also showed successful SIP registration, confirmed calls, PCMU at 8 kHz with `sendrecv` media, and nonzero RTP RX/TX counters. The retained summary deliberately omits the raw terminal trace.

## Artifacts

- `automated-verification.txt` — concise automated gate result and tested revision context.
- `manual-verification.txt` — operator acceptance checklist for registration, call directions, audio, duration, and diagnostics.
- `runtime-media.txt` — sanitized SIP/media observations from the live run.
- `login.png` — registration screen with the password masked.
- `dialer.png` — registered `Ready` state and dialer.
- `incoming-call.png` — incoming ringing state with Answer/Hang up actions.
- `active-call.png` — connected active call with running duration.
- `diagnostics.png` — connected-call state/account/peer/direction/duration diagnostics.

The screenshots are retained as GUI evidence; the text files are curated summaries rather than full terminal logs.

## Results

The recorded automated and live GUI checks passed for the documented local lab. The screenshots support the visible UI-state claims, PJSUA2 runtime observations support successful signaling/media transport, and the separate operator listening check supports the claim of clear two-way audio. These results do not guarantee identical behavior on other SIP servers, transports, audio devices, or networks.

## Security and redaction

The password is masked in retained screenshots. Do not commit SIP passwords, Digest Authorization material, raw secret-bearing SIP/PJSIP logs, SRTP inline keys, or raw packet captures. The runtime-media artifact records only the minimum signaling/media outcome needed for verification.

## Reproduction

Build and launch the real GUI backend:

```bash
export PKG_CONFIG_PATH="$HOME/.local/pjproject-2.17/lib/pkgconfig${PKG_CONFIG_PATH:+:$PKG_CONFIG_PATH}"
cmake --preset qt-pjsip-debug
cmake --build --preset qt-pjsip-debug -j
./build/qt-pjsip-debug/src/gui/linux-voip-softphone-gui
```

Use the controlled SIP lab to register, place and receive calls, verify speech in both directions, inspect Diagnostics, then run `./scripts/phase7/verify.sh` for the automated gate. Do not reproduce evidence by committing raw authentication traces.

## References

- Phase guide: `docs/phase7/README.md`
- Architecture: `docs/architecture/qt-qml-gui.md`
- Interview review: `docs/interview/phase7-questions.md`
- Automated verifier: `scripts/phase7/verify.sh`
- GUI smoke verifier: `scripts/phase7/verify-gui-smoke.sh`
- Prior audio evidence: `evidence/phase6/README.md`
