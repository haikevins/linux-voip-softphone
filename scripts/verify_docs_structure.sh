#!/usr/bin/env bash
set -euo pipefail

repo_root="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "$repo_root"

failures=0

report_failure() {
    printf 'FAIL: %s\n' "$*" >&2
    failures=$((failures + 1))
}

check_h1() {
    local file="$1"
    local regex="$2"
    local first_line
    first_line="$(head -n 1 "$file")"
    if [[ ! "$first_line" =~ $regex ]]; then
        report_failure "$file has unexpected H1: $first_line"
    fi
}

check_h2_schema() {
    local file="$1"
    shift
    local -a expected=("$@")
    local -a actual=()
    mapfile -t actual < <(grep '^## ' "$file" || true)

    if ((${#actual[@]} != ${#expected[@]})); then
        report_failure "$file has ${#actual[@]} H2 headings; expected ${#expected[@]}"
        return
    fi

    local i
    for ((i = 0; i < ${#expected[@]}; ++i)); do
        if [[ "${actual[$i]}" != "${expected[$i]}" ]]; then
            report_failure "$file H2 #$((i + 1)) is '${actual[$i]}'; expected '${expected[$i]}'"
        fi
    done
}

architecture_schema=(
    '## Status'
    '## Purpose'
    '## Scope'
    '## Architecture model'
    '## Runtime and lifecycle'
    '## Verification strategy'
    '## Design rationale'
    '## Phase boundary'
    '## References'
)

protocol_schema=(
    '## Status'
    '## Purpose'
    '## Scope'
    '## Roles and terminology'
    '## Wire model'
    '## Runtime behavior'
    '## Interactions and dependencies'
    '## Failure modes'
    '## Security considerations'
    '## Verification observations'
    '## Phase boundary'
    '## References'
)

interview_schema=(
    '## Review scope'
    '## Questions and answers'
    '## Phase boundary'
    '## References'
)

phase_schema=(
    '## Status'
    '## Objective'
    '## Scope'
    '## Deliverables'
    '## Architecture and design'
    '## Build and run'
    '## Verification'
    '## Evidence'
    '## Interview focus'
    '## Exit criteria'
    '## References'
    '## Next phase'
)

shopt -s nullglob

architecture_files=(docs/architecture/*.md)
protocol_files=(docs/protocols/*.md)
interview_files=(docs/interview/*.md)
phase_files=(docs/phase*/README.md)

((${#architecture_files[@]} > 0)) || report_failure 'no architecture documents found'
((${#protocol_files[@]} > 0)) || report_failure 'no protocol documents found'
((${#interview_files[@]} > 0)) || report_failure 'no interview documents found'
((${#phase_files[@]} > 0)) || report_failure 'no phase README documents found'

for file in "${architecture_files[@]}"; do
    check_h1 "$file" '^# Phase [0-9]+ Architecture — .+$'
    check_h2_schema "$file" "${architecture_schema[@]}"
done

for file in "${protocol_files[@]}"; do
    check_h1 "$file" '^# [A-Za-z0-9.+/-]+ Protocol — .+$'
    check_h2_schema "$file" "${protocol_schema[@]}"
done

for file in "${interview_files[@]}"; do
    if [[ "$(basename "$file")" =~ ^phase[0-9]+-questions\.md$ ]]; then
        :
    else
        report_failure "$file does not follow phaseN-questions.md naming"
    fi
    check_h1 "$file" '^# Phase [0-9]+ Interview Review — .+$'
    check_h2_schema "$file" "${interview_schema[@]}"
done

for file in "${phase_files[@]}"; do
    check_h1 "$file" '^# Phase [0-9]+ — .+$'
    check_h2_schema "$file" "${phase_schema[@]}"
done

checkbox_file="$(mktemp)"
while IFS= read -r -d '' markdown_file; do
    grep -nHE '\[[ xX]\]' "$markdown_file" >>"$checkbox_file" || true
done < <(find . -type f -name '*.md' -not -path './.git/*' -not -path './build/*' -not -path './cmake-build-*/*' -print0)

if [[ -s "$checkbox_file" ]]; then
    cat "$checkbox_file" >&2
    report_failure 'Markdown checkbox syntax is not allowed'
fi
rm -f "$checkbox_file"

if ((failures != 0)); then
    printf 'Documentation structure verification: FAIL (%d issue(s))\n' "$failures" >&2
    exit 1
fi

printf 'Documentation structure verification: PASS\n'
