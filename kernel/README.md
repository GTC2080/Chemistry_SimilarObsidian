# Chemistry_Obsidian Kernel

This directory is the sealed kernel platform container.

Current sealed milestone:

- `stage-phase2-track5-gated`

Current state:

- Phase 1 host-stable kernel baseline: complete
- Phase 2 Track 1-5: complete and gated

## Prerequisites

- Windows
- Visual Studio 2022 Build Tools with the C++ workload
- CMake 3.21 or newer

Open a Developer PowerShell for Visual Studio 2022, or any shell where `cmake` and `ctest` are already on `PATH`.

## Standard Commands

Configure the kernel:

```powershell
cmake --preset dev
```

Build the default Debug configuration:

```powershell
cmake --build --preset build-debug
```

Run the Debug test suite:

```powershell
ctest --preset test-debug
```

Build Release when needed:

```powershell
cmake --build --preset build-release
```

Build a single target from the shared tree:

```powershell
cmake --build --preset build-debug --target kernel_api_tests
```

Run the sealed kernel gate:

```powershell
cmake --build --preset build-debug --target kernel_phase_gate
```

## Output Layout

- Build tree: `out/build`
- Test binaries: `out/build/tests/<Config>/`
- Benchmark binaries: `out/build/benchmarks/<Config>/`

## Key Docs

- Docs index: `docs/README.md`
- Current stage snapshot: `docs/status/kernel-phase1-status.md`
- Release checklist: `docs/governance/release-checklist.md`
- Benchmark baselines: `docs/governance/benchmark-baselines.md`
- Public query ABI overview: `docs/surfaces/query-public-surface.md`

## Local Overrides

If you need machine-specific preset overrides, put them in `CMakeUserPresets.json`. The repository ignores that file on purpose.
