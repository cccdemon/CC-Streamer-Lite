# Claude Code Handover: Phase 04 Camera Capture

## Goal

Add webcam capture and show it as picture-in-picture over the window source.

## Current Context

- Use Media Foundation for native camera capture.
- Initial layout is fixed picture-in-picture.

## Own These Files

- New files under `src/capture/`
- New files under `src/video/` or `src/render/`
- UI device selection additions in `src/app/`

## Tasks

- Enumerate camera devices.
- List supported formats.
- Start/stop camera capture.
- Upload camera frames into compositor.
- Render fixed camera overlay.

## Acceptance Checks

- 720p30 works where supported.
- 1080p30 works where supported.
- Camera can be enabled/disabled without restart.
- Denied or missing camera gives a clear local error.

## Constraints

- Do not start camera capture without explicit user action.
- Do not log camera device names unless needed for local diagnostics.
- Keep Media Foundation code outside UI classes.

