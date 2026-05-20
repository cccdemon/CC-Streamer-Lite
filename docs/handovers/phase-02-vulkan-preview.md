# Claude Code Handover: Phase 02 Vulkan Preview Shell

## Goal

Replace the placeholder central widget with a Vulkan-backed preview surface.

## Current Context

- Vulkan is the preferred compositor backend.
- D3D11 remains necessary for Windows capture interop and fallback.
- Current UI is a simple Win32 main window.

## Own These Files

- `src/app/MainWindow.*`
- New files under `src/video/` or `src/render/`
- `CMakeLists.txt`

## Tasks

- Create a Vulkan preview widget or native child window.
- Initialize Vulkan instance, physical device, logical device, and swapchain.
- Render a clear color every frame.
- Handle resize and swapchain recreation.
- Report selected adapter and Vulkan version.

## Acceptance Checks

- Preview surface renders continuously.
- Window resize is stable.
- Missing Vulkan produces a clear error or fallback state.
- No capture/encode code is introduced here.

## Constraints

- Keep renderer lifecycle isolated from UI code.
- Do not block the Qt event loop.
- Avoid hidden dependency on a specific GPU vendor.
