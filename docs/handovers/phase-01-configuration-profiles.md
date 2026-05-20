# Claude Code Handover: Phase 01 Configuration and Profiles

## Goal

Implement profile loading, validation, saving, and secret-safe config handling.

## Current Context

- `src/config/Profile.h` has the first profile structs.
- `config/example.toml` defines expected user-facing config shape.
- Production storage should eventually use Windows Credential Manager.

## Own These Files

- `src/config/*`
- `config/example.toml`
- `src/logging/*` only for redaction integration.

## Tasks

- Add a TOML parser dependency.
- Implement load/save for `StreamProfile`.
- Validate output protocol allowlist: `srt`, `rtsp`.
- Validate URL scheme matches protocol.
- Keep `stream_key` and `srt_passphrase` optional.
- Add a secret reference abstraction for later Windows Credential Manager support.

## Acceptance Checks

- Example config loads.
- Unsupported protocol fails validation.
- Malformed URL fails validation.
- Empty stream key succeeds.
- Special characters in stream key do not corrupt URL handling.
- Raw secrets never appear in logs.

## Constraints

- Do not concatenate URLs by hand.
- Do not add raw secrets to errors or diagnostics.
- Keep RTMP/WHIP out until explicitly implemented.

