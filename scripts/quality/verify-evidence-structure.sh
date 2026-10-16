#!/usr/bin/env bash
set -euo pipefail

repo_root="$(git rev-parse --show-toplevel 2>/dev/null || true)"
if [[ -z "$repo_root" ]]; then
    echo "ERROR: run inside the linux-voip-softphone Git repository" >&2
    exit 2
fi
cd "$repo_root"

fail=0
error() {
    printf 'ERROR: %s\n' "$*" >&2
    fail=1
}

expected_phase_h2=$'## Status\n## Verification scope\n## Environment\n## Automated verification\n## Manual verification\n## Artifacts\n## Results\n## Security and redaction\n## Reproduction\n## References'
expected_scenario_h2=$'## Purpose\n## Preconditions\n## Procedure\n## Observations\n## Result\n## Artifacts\n## Security and redaction\n## References'

[[ -f evidence/README.md ]] || error "missing evidence/README.md"

for n in 0 1 2 3 4 5 6 7 8 9 10 11; do
    f="evidence/phase${n}/README.md"
    [[ -f "$f" ]] || { error "missing $f"; continue; }

    h1="$(grep -m1 '^# ' "$f" || true)"
    [[ "$h1" == "# Phase ${n} Evidence — "* ]] || error "$f has unexpected H1: $h1"

    actual="$(grep '^## ' "$f" || true)"
    [[ "$actual" == "$expected_phase_h2" ]] || {
        error "$f does not match the phase evidence H2 schema"
        printf '%s\n' "$actual" >&2
    }

    grep -Fq "docs/phase${n}/README.md" "$f" || error "$f does not reference docs/phase${n}/README.md"
done

# Phase 6 uses short sanitized summaries rather than full terminal transcripts.
for artifact in \
    automated-verification.txt \
    live-registration.txt \
    live-outgoing-call.txt \
    live-incoming-call.txt \
    phase6-summary.txt; do
    f="evidence/phase6/${artifact}"
    [[ -s "$f" ]] || error "missing or empty Phase 6 evidence artifact: $f"
done

# Phase 7 retains concise summaries plus the reviewed GUI screenshots.
for artifact in \
    automated-verification.txt \
    manual-verification.txt \
    runtime-media.txt \
    login.png \
    dialer.png \
    incoming-call.png \
    active-call.png \
    diagnostics.png; do
    f="evidence/phase7/${artifact}"
    [[ -s "$f" ]] || error "missing or empty Phase 7 evidence artifact: $f"
done

# Phase 8 retains concise concurrency/stress/TSan summaries.
for artifact in \
    automated-verification.txt \
    stress-test.txt \
    tsan-report.txt; do
    f="evidence/phase8/${artifact}"
    [[ -s "$f" ]] || error "missing or empty Phase 8 evidence artifact: $f"
done

# Phase 9 retains concise debugging/core/sanitizer summaries.
for artifact in \
    automated-verification.txt \
    gdb-core-analysis.txt \
    asan-report.txt; do
    f="evidence/phase9/${artifact}"
    [[ -s "$f" ]] || error "missing or empty Phase 9 evidence artifact: $f"
done

# Phase 10 retains concise RTSP/RTP automated and loopback integration summaries.
for artifact in \
    automated-verification.txt \
    integration-verification.txt; do
    f="evidence/phase10/${artifact}"
    [[ -s "$f" ]] || error "missing or empty Phase 10 evidence artifact: $f"
done

# Phase 11 retains final aggregate and release/package acceptance summaries.
for artifact in \
    automated-verification.txt \
    release-verification.txt; do
    f="evidence/phase11/${artifact}"
    [[ -s "$f" ]] || error "missing or empty Phase 11 evidence artifact: $f"
done

for f in evidence/phase4/registration/README.md evidence/phase4/call-flow/README.md; do
    [[ -f "$f" ]] || { error "missing $f"; continue; }
    actual="$(grep '^## ' "$f" || true)"
    [[ "$actual" == "$expected_scenario_h2" ]] || {
        error "$f does not match the evidence scenario H2 schema"
        printf '%s\n' "$actual" >&2
    }
done

legacy_re='evidence/(00-phase0|01-cpp|02-linux|03-networking|04-sip|05-pjsip)(/|$)'
if git grep -nE "$legacy_re" -- . ':!scripts/quality/verify-evidence-structure.sh' >/tmp/voip-evidence-legacy.$$ 2>/dev/null; then
    cat /tmp/voip-evidence-legacy.$$ >&2
    error "legacy evidence path references remain"
fi
rm -f /tmp/voip-evidence-legacy.$$

for d in evidence/00-phase0 evidence/01-cpp evidence/02-linux evidence/03-networking evidence/04-sip evidence/05-pjsip; do
    [[ ! -e "$d" ]] || error "legacy evidence directory remains: $d"
done

# Historical Git state is reproducible from Git itself and is not retained as
# phase verification evidence.
obsolete_vcs_artifacts=(
    git-history.txt
    git-status.txt
    git-diff-check.log
    git-log.txt
)
for name in "${obsolete_vcs_artifacts[@]}"; do
    while IFS= read -r f; do
        [[ -z "$f" ]] || error "obsolete VCS bookkeeping artifact remains: $f"
    done < <(find evidence -type f -name "$name" -print)
done

# Ensure evidence collectors cannot silently recreate the removed bookkeeping.
if git grep -nE 'git-history\.txt|git-status\.txt|git-diff-check\.log|git-log\.txt' \
    -- 'scripts/phase*/collect-evidence.sh' >/tmp/voip-evidence-producers.$$ 2>/dev/null; then
    cat /tmp/voip-evidence-producers.$$ >&2
    error "evidence collector still produces obsolete VCS bookkeeping"
fi
rm -f /tmp/voip-evidence-producers.$$

for d in evidence/phase4/artifacts/final evidence/phase4/artifacts/protocol; do
    [[ ! -e "$d" ]] || error "obsolete Phase 4 evidence bucket remains: $d"
done

if grep -R -nE --include='*.md' '\[[ xX]\]' evidence >/tmp/voip-evidence-checkbox.$$ 2>/dev/null; then
    cat /tmp/voip-evidence-checkbox.$$ >&2
    error "Markdown checkbox syntax remains in evidence documentation"
fi
rm -f /tmp/voip-evidence-checkbox.$$

if find evidence -type f \( -name '*.pcap' -o -name '*.pcapng' \) -print -quit | grep -q .; then
    find evidence -type f \( -name '*.pcap' -o -name '*.pcapng' \) -print >&2
    error "packet-capture files must not be committed under evidence/"
fi

# Detect common secret-bearing SIP material without flagging policy text alone.
if grep -R -nEi '^[[:space:]]*Authorization:[[:space:]]*Digest[[:space:]]' evidence >/tmp/voip-evidence-auth.$$ 2>/dev/null; then
    cat /tmp/voip-evidence-auth.$$ >&2
    error "Digest Authorization material detected under evidence/"
fi
rm -f /tmp/voip-evidence-auth.$$

if grep -R -nEi 'a=crypto:.*inline:' evidence >/tmp/voip-evidence-srtp.$$ 2>/dev/null; then
    cat /tmp/voip-evidence-srtp.$$ >&2
    error "SRTP inline key material detected under evidence/"
fi
rm -f /tmp/voip-evidence-srtp.$$

if (( fail != 0 )); then
    echo "Evidence structure verification: FAIL" >&2
    exit 1
fi

echo "Evidence structure verification: PASS"
