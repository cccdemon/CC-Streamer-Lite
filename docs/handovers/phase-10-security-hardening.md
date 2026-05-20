# Claude Code Handover: Phase 10 Security Hardening

## Goal

Harden secret handling, dependency loading, and diagnostics before release.

## Current Context

- Plan requires no secret leakage in logs, UI, telemetry, errors, or crash dumps.
- Production profiles should support Windows Credential Manager.

## Own These Files

- `src/security/*`
- `src/config/*`
- `src/logging/*`
- Installer/release scripts if present.

## Tasks

- Implement Windows Credential Manager backend.
- Store only secret references in production profiles.
- Add redaction tests.
- Configure safe DLL search behavior.
- Document dependency verification.
- Add crash/diagnostic scrubbing policy.

## Acceptance Checks

- Raw secrets are not required on disk.
- Logs and UI redact credentials.
- Unsupported protocols are refused.
- Overlong SRT stream IDs fail locally.
- DLL loading uses controlled paths.

## Constraints

- Do not weaken development ergonomics by bypassing production checks silently.
- Keep secret APIs explicit and testable.
- Avoid adding MediaMTX control API permissions unless required.

