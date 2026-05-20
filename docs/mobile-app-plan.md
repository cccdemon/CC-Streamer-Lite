# Mobile App Plan

## Goal

Create companion mobile apps for iOS and Android that can control CC-Streamer and optionally act as a camera source later.

## Recommended Stack

- Flutter for one shared iOS/Android codebase.
- Native platform channels only where needed for camera/audio device access.
- HTTPS/WebSocket control API to the desktop app.

## Phase 1: Remote Control

- Discover desktop app on LAN by manual IP first.
- Show stream status.
- Start/stop Window WebRTC stream.
- Start/stop Cam WebRTC stream.
- Edit WebRTC/WHIP endpoints.
- Select encoder preset and YUV range/matrix.

## Phase 2: Mobile Camera Source

- Use phone camera as an extra source.
- Send video to desktop by WebRTC.
- Desktop app receives phone source and composites/streams it.

## Phase 3: Direct Mobile Streaming

- Stream directly from phone to MediaMTX WHIP endpoint.
- H.264 hardware encoder through platform APIs.
- Opus audio where platform support allows it.

## Security

- Pair phone and desktop with a one-time code.
- Use local API tokens after pairing.
- Never expose stream keys in mobile logs.
- Require explicit user confirmation before starting phone camera/microphone.

## Notes

Mobile should not be the first streaming implementation. The desktop app still needs stable capture, color conversion, encoding, and WHIP publishing first.

