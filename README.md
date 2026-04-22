# Chemistry_Obsidian

This repository is organized as a small monorepo.

## Layout

- `kernel/`
  - the sealed kernel platform
- `apps/electron/`
  - placeholder host entry for future Electron integration
- `docs/`
  - monorepo-level structure and integration documents

## Canonical Commands

All current build, test, benchmark, and gate commands run from `kernel/`.

```powershell
cd kernel
cmake --preset dev
cmake --build --preset build-debug
ctest --preset test-debug
cmake --build --preset build-debug --target kernel_phase_gate
```

Do not run the kernel build from the repository root.

## Entry Docs

- Repo structure: `docs/repo-structure.md`
- Integration plan: `docs/integration-plan.md`
- Kernel entry: `kernel/README.md`

## Current Sealed Node

- sealed tag: `stage-phase2-track5-gated`
- kernel milestone:
  - Phase 1 host-stable kernel baseline complete
  - Phase 2 Track 1-5 complete and gated
