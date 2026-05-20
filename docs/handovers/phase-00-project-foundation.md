# Claude Code Handover: Phase 00 Project Foundation

## Goal

Make the repository build as a Windows x64 native C++ application shell.

## Current Context

- `CMakeLists.txt` defines `CCStreamer`.
- The current first UI shell is plain Win32 so the available Visual Studio Build Tools can compile the project without Qt.
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
- Confirm Debug and Release presets.
- Add missing install/runtime-copy steps only if needed for local execution.

## Acceptance Checks

- `cmake --preset vs2019-x64-debug` succeeds.
- `cmake --build --preset vs2019-x64-debug` succeeds.
- The app launches and shows the main window.
- No capture or streaming work is introduced in this phase.

## Constraints

- Keep this phase focused on the app shell.
- Do not add FFmpeg, capture, or streaming implementation yet.
- Preserve the project layout from `IMPLEMENTATION_PLAN.md`.
