# CC-Streamer Plan

## Goal

Build a lightweight Windows x64 native streaming app, similar in purpose to OBS/Meld Studio but focused on a smaller feature set:

- Window capture with system/application sound.
- Camera capture.
- Audio encoded as Opus.
- Stream to two configurable endpoints.
- Optional video re-encode/downscale from 4K to 1080p or 1440p.
- Video encoded as H.264 by default, with hardware acceleration preferred when available.
- Primary target: configurable MediaMTX streaming server.

## Recommended Technical Direction

Use a native C++ application with:

- UI: Qt 6 or WinUI 3.
- Capture:
  - Windows Graphics Capture for window capture.
  - WASAPI loopback for system audio.
  - Media Foundation for webcam capture.
- Rendering/compositing:
  - Prefer Vulkan for rendering and compositing.
  - Use Direct3D 11 interop where Windows capture APIs require D3D textures.
  - Keep Direct3D 11 as a fallback renderer if Vulkan initialization or adapter compatibility fails.
  - Optional simple scene graph: window source, camera source, audio meters, preview.
- Encoding:
  - FFmpeg libraries for muxing, transport, Opus audio, and fallback software video encode.
  - Hardware video encode where available: NVENC, Intel QSV, AMD AMF.
  - Default video codec: H.264.
  - Default color pipeline: YUV BT.709 limited range, configurable to BT.601 and full range.
- Streaming protocol:
  - Prefer SRT or RTSP/RTP for MediaMTX when Opus is required.
- WHIP/WebRTC is a good later target if browser playback and low latency are important.
  - RTMP can be supported later if the target server/client stack supports Enhanced RTMP with Opus. Do not make it the first ingest path because broader ecosystem compatibility is less predictable than SRT or RTSP for Opus.

## High-Level Architecture

```text
UI / Settings
  |
  +-- Source Manager
  |     +-- Window Capture Source
  |     +-- Camera Capture Source
  |     +-- WASAPI Audio Source
  |
  +-- Preview Renderer
  |     +-- Vulkan compositor
  |     +-- D3D11 capture interop
  |
  +-- Pipeline Controller
        +-- Video scaler
        +-- Audio mixer
        +-- Encoder profile
        +-- Output A publisher
        +-- Output B publisher
```

## MVP Scope

Version 0.1 should include:

- Single window capture source.
- Single camera source.
- System audio capture through WASAPI loopback.
- One preview canvas.
- One fixed layout: window full frame, camera picture-in-picture.
- Video encode to H.264.
- Audio encode to Opus.
- Stream to one MediaMTX endpoint.
- Config file for server URL, stream path, resolution, bitrate, FPS, and encoder.

Version 0.2 should add:

- Two simultaneous stream endpoints.
- Per-target enable/disable.
- Shared encode when both endpoints use the same profile.
- Separate encode pipelines when endpoint profiles differ.
- 1080p and 1440p downscale presets.

Version 0.3 should add:

- Source selection UI.
- Audio device selection.
- Basic scene editing: position, scale, crop camera overlay.
- Start/stop status, reconnect handling, dropped-frame counters.
- Save/load profiles.

## MediaMTX Endpoint Model

Example config values:

```toml
[video]
width = 1920
height = 1080
fps = 30
bitrate_kbps = 6000
encoder = "auto"

[audio]
codec = "opus"
bitrate_kbps = 128
sample_rate = 48000
channels = 2

[[outputs]]
name = "primary"
enabled = true
protocol = "srt"
url = "srt://stream.example.com:8890?streamid=publish:live/main"
stream_key = ""
srt_passphrase = ""

[[outputs]]
name = "backup"
enabled = false
protocol = "srt"
url = "srt://backup.example.com:8890?streamid=publish:live/main"
stream_key = ""
srt_passphrase = ""
```

If MediaMTX is configured for RTSP instead:

```toml
protocol = "rtsp"
url = "rtsp://stream.example.com:8554/live/main"
stream_key = ""
```

`stream_key` is optional. If it is empty, the app uses the URL as-is. If it is set, the output module appends or injects it according to the selected protocol and server profile. This allows both URL-only MediaMTX setups and hosted-style ingest setups that separate server URL from stream key.

For MediaMTX WebRTC ingest, configure WHIP endpoints with the `/whip` suffix, for example `http://stream.example.com:8889/live/window/whip` and `http://stream.example.com:8889/live/camera/whip`.

Secrets such as `stream_key`, `srt_passphrase`, usernames, passwords, and tokens must not be written to logs or crash reports. The config file may reference secrets during development, but production builds should support Windows Credential Manager so profiles can store only secret references.

## Security Requirements

- Treat configured endpoints as sensitive. Only allow explicit streaming protocols: `srt`, `rtsp`, and later `rtmp` or `whip` when implemented.
- Parse and build endpoint URLs with a structured URL library. Do not concatenate `stream_key`, usernames, passwords, or query strings by hand.
- Redact credentials in UI status, logs, telemetry, error dialogs, and crash dumps.
- Validate SRT `streamid` length before connecting. Keep generated stream IDs under the protocol/server limit and fail with a clear local error.
- Support encrypted SRT by allowing an optional passphrase and reject weak passphrases when encryption is enabled.
- Prefer secure transports where available: SRT encryption, RTSPS, or WHIP over HTTPS.
- Require explicit user selection for window, camera, and audio devices before capture starts.
- Show a clear live/capture indicator while recording or streaming.
- Avoid persisting selected window titles, device names, or recent URLs unless the user saves a profile.
- Handle capture denial, protected windows, and lost devices without retry loops that spam logs or leak state.
- Pin or verify third-party binary dependencies, especially FFmpeg builds and GPU encoder runtime loading.
- Load DLLs from controlled application directories and use safe Windows DLL search settings.
- Keep the MediaMTX control API out of the app unless needed. If added later, require separate credentials and least-privilege access.

## Main Engineering Risks

- Opus transport compatibility depends on the chosen MediaMTX ingest protocol.
- Window capture and audio capture must stay synchronized under load.
- Dual output can double CPU/GPU load if profiles differ.
- Hardware encoder availability varies by GPU and driver.
- Protected windows or DRM video cannot reliably be captured.
- Vulkan/D3D11 interop can add complexity and copy overhead if zero-copy sharing is not available on the target GPU.
- Plaintext profile files can leak stream credentials if secret storage is not implemented.
- A configurable endpoint can accidentally stream private screen/audio content to the wrong server if confirmation and profile handling are weak.

## Validation Test Plan

### Plan-Level Checks

- Confirm MediaMTX ingest protocol and codec support for the deployed server version.
- Confirm whether the target clients need RTSP, SRT, WebRTC, HLS, or RTMP playback.
- Confirm whether Opus is mandatory for ingest, playback, or both.
- Confirm whether two outputs must use identical encode settings or can use separate resolutions/bitrates.

### Security Tests

- Save profiles with `stream_key`, `srt_passphrase`, username, password, and token values, then verify logs and UI never expose the raw secret.
- Test malformed URLs, unsupported schemes, embedded credentials, duplicated query parameters, overlong SRT stream IDs, and special characters in stream keys.
- Test SRT with encryption enabled and disabled.
- Test permission denial for camera, microphone, screen/window capture, and audio loopback.
- Test reconnect behavior with bad credentials, unreachable hosts, refused connections, and mid-stream network loss.
- Verify crash dumps and diagnostics are either disabled by default or scrubbed of endpoint credentials.

### Functional Tests

- Capture a normal window, a minimized window, a resized window, and a protected/DRM window.
- Capture system audio and verify A/V sync after 10, 30, and 120 minutes.
- Capture a webcam at common formats: 720p30, 1080p30, 1080p60.
- Stream H.264 + Opus to one MediaMTX SRT endpoint.
- Stream H.264 + Opus to two endpoints with the same encode profile.
- Stream to two endpoints with different output profiles and confirm separate encoders are used.
- Downscale 4K input to 1080p and 1440p and verify bitrate, FPS, and aspect ratio.
- Force hardware encoder failure and verify fallback to software encode or a clear local error.
- Run with Vulkan unavailable and verify Direct3D 11 fallback.
- Run with D3D11/Vulkan interop unavailable and verify the app fails gracefully or uses a copy path.

## Suggested Repository Structure

```text
CC-Streamer/
  CMakeLists.txt
  README.md
  PLAN.md
  src/
    app/
    capture/
    audio/
    video/
    encode/
    output/
    ui/
  third_party/
  config/
    example.toml
  docs/
```

## Build Milestones

1. Create Windows x64 CMake project and app shell.
2. Add Vulkan preview window.
3. Implement Windows Graphics Capture source.
4. Implement WASAPI loopback audio capture.
5. Implement Media Foundation camera source.
6. Add FFmpeg encode/mux pipeline with H.264 + Opus.
7. Publish to one MediaMTX SRT or RTSP endpoint.
8. Add downscale presets.
9. Add second endpoint output.
10. Add reconnect/status/error handling.

## Initial Decision

Start with SRT to MediaMTX for ingest unless the server is already standardized on RTSP or WHIP. SRT is practical for native contribution feeds, works well with FFmpeg, supports modern codecs cleanly through MPEG-TS or Matroska-style pipelines, and fits the two-endpoint streaming requirement.

For rendering, start with Vulkan as the primary compositor. Windows Graphics Capture commonly delivers D3D-backed frames, so the capture layer should isolate D3D11 interop and hand frames to the Vulkan renderer through a narrow texture-transfer boundary. This keeps the app Vulkan-first without fighting the native Windows capture stack.
