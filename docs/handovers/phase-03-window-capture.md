# Claude Code Handover: Phase 03 Window Capture

## Goal

Capture a user-selected window and display it in the preview.

## Current Context

- Use Windows Graphics Capture.
- Captured frames will likely be D3D11-backed.
- The renderer is Vulkan-first, so interop must stay isolated.

## Own These Files

- New files under `src/capture/`
- New files under `src/video/` or `src/render/` for texture transfer
- UI picker additions in `src/app/`

## Tasks

- Implement window picker UI.
- Implement Windows Graphics Capture session lifecycle.
- Receive frames as D3D11 textures.
- Transfer frames to the Vulkan preview path.
- Handle closed, resized, minimized, and protected windows.

## Acceptance Checks

- Selected normal window appears in preview.
- Resize updates correctly.
- Closing the captured window is recoverable.
- Protected content does not crash the app.
- Capture starts only after explicit user selection.

## Constraints

- Do not persist selected window titles unless profile saving explicitly requests it.
- Do not log window titles by default.
- Keep D3D11 interop behind a narrow interface.

