# Security Policy

## Supported version

Security fixes are evaluated against the current `main` branch and the latest published release.

## Reporting a vulnerability

Please do not open a public issue for a vulnerability that could expose credentials, authentication material, private network details, or another user's data.

Use GitHub's private vulnerability reporting feature for this repository when available. If private reporting is not available, contact the repository maintainer through the contact information on the GitHub profile and provide only the minimum information needed to reproduce the issue safely.

Include:

- the affected component;
- a concise description of the impact;
- reproduction steps or a minimal proof of concept;
- the affected revision or release;
- any mitigation you have already identified.

Do not include real SIP passwords, Digest authorization headers, SRTP keying material, private packet captures, or unrelated personal data in a report.

## Scope

This repository is an engineering portfolio and lab project, not a production telephony service. Security reports are still welcome when they concern code in the repository, build/release artifacts, dependency integration, credential handling, or unsafe runtime behavior.
