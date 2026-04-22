# Chemistry_Obsidian

This repository uses one checked-in CMake entry point:

- Configure into `out/build`
- Build and test through `CMakePresets.json`
- Do not create a top-level `build/` directory

## Prerequisites

- Windows
- Visual Studio 2022 Build Tools with the C++ workload
- CMake 3.21 or newer

Open a Developer PowerShell for Visual Studio 2022, or any shell where `cmake` and `ctest` are already on `PATH`.

## Standard Commands

Configure the repo:

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

## Output Layout

- Build tree: `out/build`
- Test binaries: `out/build/tests/<Config>/`
- Benchmark binaries: `out/build/benchmarks/<Config>/`

## Key Docs

- Docs index: `docs/README.md`
- Runtime and Phase 1 snapshot: `docs/status/kernel-phase1-status.md`
- Public query ABI contract: `docs/surfaces/query-public-surface.md`

## Local Overrides

If you need machine-specific preset overrides, put them in `CMakeUserPresets.json`. The repository ignores that file on purpose.
