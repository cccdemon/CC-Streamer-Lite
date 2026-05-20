# Claude Code Handover: Phase 00 Project Foundation

## Goal

Make the repository build as a Windows x64 native C++ application shell.

## Current Context

- `CMakeLists.txt` defines `CCStreamer`.
- Qt 6 Widgets is the chosen first UI stack.
- Vulkan is detected at configure time and exposed through `CCSTREAMER_HAS_VULKAN`.
- `src/app`, `src/config`, and `src/logging` contain initial app shell code.

## Own These Files

- `CMakeLists.txt`
- `CMakePresets.json`
- `src/main.cpp`
- `src/app/*`
- `docs/dev-setup.md`

## Tasks

- Verify CMake configure/build on Windows x64.
- Fix Qt autogen/MOC issues if needed.
- Confirm Debug and Release presets.
- Add missing install/runtime-copy steps only if needed for local execution.

## Acceptance Checks

- `cmake --preset windows-debug` succeeds.
- `cmake --build --preset windows-debug` succeeds.
- The app launches and shows the main window.
- No capture or streaming work is introduced in this phase.

## Constraints

- Keep this phase focused on the app shell.
- Do not add FFmpeg, capture, or streaming implementation yet.
- Preserve the project layout from `IMPLEMENTATION_PLAN.md`.

