# Claude Code Handover: Phase 06 Compositor and Scaling

## Goal

Compose window + camera and produce output frames at source, 1080p, or 1440p.

## Current Context

- Vulkan is the primary renderer/compositor.
- Initial scene is fixed: window full frame, camera picture-in-picture.

## Own These Files

- `src/video/*`
- `src/render/*`
- Any shader files added for scaling/color conversion.

## Tasks

- Add a simple scene graph.
- Implement source transforms.
- Implement Vulkan scaling shaders.
- Produce frames at selected output resolution.
- Add frame pacing and dropped-frame counters.

## Acceptance Checks

- 4K source downscales to 1080p.
- 4K source downscales to 1440p.
- Aspect ratio is preserved.
- FPS matches profile.
- Dropped frames are counted.

## Constraints

- Do not hard-code GPU vendor paths.
- Keep encode-facing output frame API stable.
- Preserve fallback path for missing Vulkan/interoperability.

