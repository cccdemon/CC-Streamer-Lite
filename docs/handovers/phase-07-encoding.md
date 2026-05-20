# Claude Code Handover: Phase 07 Encoding

## Goal

Encode composed video as H.264 and captured audio as Opus.

## Current Context

- FFmpeg libraries are the selected encoding/muxing foundation.
- Hardware encode should be auto-selected where available.
- Software fallback is required.

## Own These Files

- New files under `src/encode/`
- `CMakeLists.txt`
- Dependency documentation in `docs/dev-setup.md`

## Tasks

- Add FFmpeg development dependency.
- Implement encoder wrapper.
- Implement H.264 hardware auto mode.
- Implement software H.264 fallback.
- Implement Opus audio encode.
- Add local file output test path before network streaming.

## Acceptance Checks

- Local file contains H.264 + Opus.
- Resolution, FPS, bitrate, and audio settings match profile.
- Hardware encoder is used when available.
- Software fallback works.
- Encoder errors redact secrets.

## Constraints

- Keep FFmpeg types out of UI code.
- Do not make network streaming a prerequisite for encoder testing.
- Verify FFmpeg binary/source provenance before release.

