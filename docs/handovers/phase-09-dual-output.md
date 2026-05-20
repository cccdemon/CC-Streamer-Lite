# Claude Code Handover: Phase 09 Dual Output

## Goal

Support two simultaneous configurable stream outputs.

## Current Context

- Outputs are configured as an array in TOML.
- Matching profiles should share an encode pipeline.
- Different profiles require separate encoders.

## Own These Files

- `src/output/*`
- `src/encode/*`
- `src/config/*`
- UI output status code in `src/app/`

## Tasks

- Add two output slots.
- Add per-output enable/disable.
- Implement shared encoded packet fan-out for matching profiles.
- Implement separate encode pipelines for differing profiles.
- Track reconnect/error state independently per output.

## Acceptance Checks

- One output enabled works.
- Two outputs enabled work.
- Backup failure does not stop primary.
- Matching profiles use shared encode.
- Different profiles use separate encode.

## Constraints

- Keep per-output secrets isolated.
- Do not let one output's retries stall the other.
- Surface per-output status clearly.

