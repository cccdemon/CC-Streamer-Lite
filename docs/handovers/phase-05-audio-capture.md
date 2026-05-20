# Claude Code Handover: Phase 05 Audio Capture

## Goal

Capture system/application sound for streaming.

## Current Context

- Use WASAPI loopback first.
- Audio output target is Opus at 48 kHz stereo.

## Own These Files

- New files under `src/audio/`
- UI meter/status additions in `src/app/`

## Tasks

- Implement WASAPI loopback capture.
- Convert/resample to 48 kHz stereo if needed.
- Add monotonic timestamps.
- Add an audio level meter.
- Handle silence, denial, and device loss.

## Acceptance Checks

- System audio is captured.
- Silence does not break the pipeline.
- Meter updates in real time.
- Timestamps are monotonic.
- Device loss is recoverable.

## Constraints

- Do not capture microphone input unless added explicitly.
- Avoid logging device names by default.
- Keep audio capture independent from encoding.

