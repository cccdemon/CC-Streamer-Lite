# Claude Code Handover: Phase 11 Installer and Release

## Goal

Package the app for Windows x64 release.

## Current Context

- The app should support a portable ZIP or installer.
- Runtime dependencies include Qt, FFmpeg, and possibly Vulkan-related runtime assumptions.

## Own These Files

- Release/package scripts.
- `CMakeLists.txt`
- `docs/*`
- Any installer metadata.

## Tasks

- Add release build preset verification.
- Bundle required runtime DLLs.
- Add first-run profile creation.
- Add user documentation.
- Add smoke test script for clean machine validation.

## Acceptance Checks

- Clean Windows x64 machine can launch the app.
- Software encode works without GPU encoder.
- Missing Vulkan shows fallback or clear error.
- Edited MediaMTX example profile streams successfully.

## Constraints

- Do not include unverified third-party binaries.
- Do not ship example secrets.
- Keep logs and crash dumps out of release package output.

