# Integration Plan

## Current Split

- `kernel/`
  - sealed kernel platform
- `apps/electron/`
  - future Electron host entry

## Responsibility Split

- kernel work continues inside `kernel/`
- Electron host integration lands under `apps/electron/src/main/` and `apps/electron/src/preload/`
- renderer work lands under `apps/electron/src/renderer/`

## Current Rule

This repository reorganization does not start Electron implementation.

It only reserves the application boundary so later host integration can proceed without reopening the sealed kernel platform.

## Canonical Build Rule

Until Electron host integration actually starts:

- all validation still runs from `kernel/`
- all sealed-node verification still uses:
  - `ctest`
  - benchmark gates
  - `kernel_phase_gate`
