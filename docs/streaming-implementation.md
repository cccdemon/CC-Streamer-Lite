# Streaming Implementation Notes

## Current State

The GUI has WHIP/WebRTC endpoint fields and encoder controls, but the binary does not yet include a WebRTC publisher.

## Required Dependency

For WHIP/WebRTC output, add one of:

- libdatachannel
- Google's native WebRTC SDK
- GStreamer with webrtcbin

Recommended first choice: `libdatachannel`, because it is smaller than the full WebRTC SDK and fits a native lightweight app.

## Desktop Streaming Pipeline

```text
Window capture frames
Camera capture frames
WASAPI audio frames
        |
Compositor / scaler
        |
H.264 encoder + Opus encoder
        |
WHIP publisher
        |
MediaMTX / live/window/whip and live/camera/whip
```

## Implementation Phases

1. Add frame pipeline types for BGRA/YUV video frames and PCM audio frames.
2. Replace DWM thumbnail preview with Windows Graphics Capture frames.
3. Add WASAPI loopback audio capture.
4. Add H.264 encoder:
   - Media Foundation hardware encoder first.
   - Later NVENC/QSV/AMF selection.
5. Add Opus audio encoder.
6. Add WHIP publisher dependency.
7. Wire `Start Stream` to publish selected streams.

## Current `Start Stream` Behavior

Until the WHIP dependency is linked, `Start Stream` performs only preflight/status reporting.

