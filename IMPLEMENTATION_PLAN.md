# CC-Streamer Implementation Plan

## Target

Build a lightweight native Windows x64 streaming application with:

- Vulkan-first preview/compositing.
- Windows window capture.
- Camera capture.
- System/application audio capture.
- H.264 video encode.
- Opus audio encode.
- Optional downscale to 1080p or 1440p.
- One or two configurable MediaMTX outputs.
- Optional stream key and SRT passphrase handling.

## Phase 0: Project Foundation

### Deliverables

- CMake-based Windows x64 C++ project.
- Application entry point.
- Basic logging with secret redaction support from day one.
- Dependency strategy for:
  - Qt 6 or WinUI 3.
  - Vulkan SDK.
  - FFmpeg shared libraries.
  - TOML parser.
  - Structured URL parser.
- `config/example.toml`.
- `docs/dev-setup.md`.

### Recommended Choice

Use CMake + Qt 6 for the first version. Qt gives a mature desktop UI, settings dialogs, file pickers, device lists, and a stable event loop without needing to build too much Windows UI plumbing manually.

### Acceptance Checks

- App launches on Windows x64.
- Empty main window opens.
- Logs write to a local app log directory.
- Known secret-like values are redacted in logs.
- Build can run in Debug and Release.

## Phase 1: Configuration and Profiles

### Deliverables

- Profile model:
  - Video width, height, FPS, bitrate, encoder.
  - Audio codec, bitrate, sample rate, channels.
  - Output list with name, enabled, protocol, URL, optional stream key, optional SRT passphrase.
- TOML load/save.
- URL validation and protocol allowlist.
- Secret abstraction:
  - Plain config for development.
  - Interface ready for Windows Credential Manager.

### Acceptance Checks

- Valid example config loads.
- Invalid protocol is rejected.
- Malformed URL is rejected.
- Empty stream key is accepted.
- Stream key with special characters is accepted without corrupting the URL.
- Logs never print raw stream keys, passphrases, passwords, or tokens.

## Phase 2: Vulkan Preview Shell

### Deliverables

- Vulkan instance/device initialization.
- Swapchain attached to the app window.
- Render loop.
- Clear-color preview surface.
- Renderer capability report:
  - Selected adapter.
  - Vulkan version.
  - Required extensions.
- Direct3D 11 fallback decision path.

### Acceptance Checks

- Vulkan preview renders continuously.
- App reports a clear error if Vulkan is unavailable.
- App can select fallback path when Vulkan initialization fails.
- Resizing the window recreates the swapchain cleanly.

## Phase 3: Window Capture

### Deliverables

- Window picker UI.
- Windows Graphics Capture integration.
- D3D11 texture receiving path.
- D3D11-to-Vulkan transfer boundary.
- Basic frame timing.
- Protected-window and lost-window handling.

### Acceptance Checks

- Selected normal window appears in preview.
- Resized captured window updates correctly.
- Closed captured window produces a recoverable error.
- Protected/blocked content does not crash the app.
- Capture does not start until user selects a window.

## Phase 4: Camera Capture

### Deliverables

- Camera device enumeration.
- Camera format selection.
- Media Foundation capture pipeline.
- Upload camera frames into the compositor.
- Fixed picture-in-picture placement.

### Acceptance Checks

- Camera preview works at 720p30 and 1080p30 where supported.
- Camera can be enabled/disabled without restarting the app.
- Missing or denied camera access produces a clear local error.
- Camera picture-in-picture remains stable while resizing the app.

## Phase 5: Audio Capture

### Deliverables

- WASAPI loopback capture.
- Optional input device capture if needed later.
- Audio resampling to 48 kHz.
- Stereo output path.
- Audio level meter.
- Timestamping for A/V sync.

### Acceptance Checks

- System audio is captured.
- Silence is handled without encoder errors.
- Audio level meter responds in real time.
- Audio timestamps remain monotonic.
- Capture denial or device loss is recoverable.

## Phase 6: Compositor and Scaling

### Deliverables

- Scene graph with:
  - Full-frame window source.
  - Camera picture-in-picture source.
- Vulkan shaders for color conversion and scaling.
- Output frame generation at target resolution.
- Presets:
  - Source resolution.
  - 1920x1080.
  - 2560x1440.
- Frame pacing and dropped-frame accounting.

### Acceptance Checks

- 4K source can be downscaled to 1080p.
- 4K source can be downscaled to 1440p.
- Aspect ratio is preserved.
- Output frame rate matches profile.
- Dropped frames are counted and visible in status.

## Phase 7: Encoding

### Deliverables

- FFmpeg encoder wrapper.
- H.264 video encode:
  - Hardware auto mode.
  - Software fallback.
- Opus audio encode.
- A/V muxing for selected output protocol.
- Encoder diagnostics.

### Acceptance Checks

- H.264 + Opus encode works for local file test output.
- Hardware encoder is used when available.
- Software fallback works when hardware encode fails.
- Bitrate, FPS, resolution, and audio settings match profile.
- Encoder errors do not leak endpoint secrets.

## Phase 8: Single MediaMTX Output

### Deliverables

- Output publisher abstraction.
- SRT publisher first.
- RTSP publisher second if needed by the deployed server.
- Optional stream key injection.
- Optional SRT passphrase.
- Reconnect policy with backoff.
- Start/stop streaming controls.

### Acceptance Checks

- App streams H.264 + Opus to one MediaMTX SRT endpoint.
- MediaMTX receives the stream under the configured path.
- Wrong credentials fail clearly and do not loop aggressively.
- Network loss triggers bounded reconnect attempts.
- UI clearly shows live/stopped/error state.

## Phase 9: Dual Output

### Deliverables

- Two output slots.
- Per-output enable/disable.
- Shared encoder fan-out when both outputs use identical media settings.
- Separate encoder pipelines when output profiles differ.
- Independent reconnect/error state per output.

### Acceptance Checks

- One output enabled works.
- Two outputs enabled work.
- Failure of backup output does not stop primary output.
- Shared encode path is used for matching profiles.
- Separate encode path is used for different profiles.

## Phase 10: Security Hardening

### Deliverables

- Windows Credential Manager backend.
- Secret references in profile files.
- Log redaction tests.
- Safe DLL search configuration.
- Dependency verification notes.
- Crash/diagnostic scrubbing policy.

### Acceptance Checks

- Production profile does not need raw stream secrets on disk.
- Logs and UI redact credentials.
- DLLs are loaded only from controlled paths.
- App refuses unsupported protocols.
- SRT stream IDs over server/protocol limits are rejected locally.

## Phase 11: Installer and Release

### Deliverables

- Release build preset.
- Windows installer or portable ZIP.
- Bundled runtime dependencies.
- First-run config/profile creation.
- Basic user documentation.

### Acceptance Checks

- Clean Windows x64 machine can run the app.
- Missing GPU encoder still allows software encode.
- Missing Vulkan support shows fallback or actionable error.
- MediaMTX example profile works after editing endpoint values.

## Suggested First Sprint

1. Create CMake + Qt skeleton.
2. Add config/profile model and TOML parsing.
3. Add redacting logger.
4. Add Vulkan preview window.
5. Add example config.

The first sprint should avoid capture and encoding until the app shell, config model, logging policy, and renderer lifecycle are stable.

## Definition of Done for MVP

The MVP is complete when a user can:

1. Select a window.
2. Select a camera.
3. Capture system audio.
4. Preview the composed scene.
5. Choose 1080p or 1440p output.
6. Start streaming H.264 + Opus to one MediaMTX SRT endpoint.
7. Stop streaming cleanly.
8. Confirm secrets are not exposed in logs or profile output.
