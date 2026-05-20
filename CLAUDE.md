# Claude Code Entry Point

This repository implements CC-Streamer, a lightweight Windows x64 streaming app.

Start here:

1. Read `PLAN.md`.
2. Read `IMPLEMENTATION_PLAN.md`.
3. Pick the current phase from `docs/handovers/README.md`.
4. Follow the phase handover exactly, including file ownership and acceptance checks.

Current phase:

- Phase 00 Project Foundation.

Important constraints:

- Preserve secret redaction behavior.
- Do not log stream keys, SRT passphrases, usernames, passwords, or tokens.
- Keep Vulkan as the preferred compositor.
- Keep D3D11 isolated to capture interop/fallback paths.
- Use structured URL handling for endpoints.
- Do not implement later phases inside earlier handovers unless explicitly requested.

