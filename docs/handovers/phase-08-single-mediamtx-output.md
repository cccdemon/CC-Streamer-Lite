# Claude Code Handover: Phase 08 Single MediaMTX Output

## Goal

Stream H.264 + Opus to one MediaMTX endpoint.

## Current Context

- SRT is the first ingest target.
- RTSP can be added if the deployed MediaMTX server requires it.
- Stream key and SRT passphrase are optional.

## Own These Files

- New files under `src/output/`
- `src/config/*`
- `src/logging/*` only for redaction integration.

## Tasks

- Add output publisher abstraction.
- Implement SRT publishing.
- Inject optional stream key safely through structured URL handling.
- Add optional SRT passphrase.
- Add bounded reconnect with backoff.
- Add start/stop controls.

## Acceptance Checks

- MediaMTX receives one SRT stream.
- Wrong credentials fail clearly.
- Network loss triggers bounded reconnect.
- UI shows live/stopped/error state.
- Logs redact endpoint credentials.

## Constraints

- Do not hand-concatenate endpoint URLs.
- Do not retry aggressively.
- Do not block preview/capture while reconnecting.

