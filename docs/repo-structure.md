# Repo Structure

## Purpose

This repository uses a two-layer structure:

- `kernel/`
  - the sealed kernel platform and its full validation surface
- `apps/`
  - future application entrypoints built on top of the sealed kernel

## Current Tree

- `kernel/`
  - `CMakeLists.txt`
  - `CMakePresets.json`
  - `benchmarks/`
  - `cmake/`
  - `docs/`
  - `include/`
  - `src/`
  - `tests/`
  - `third_party/`
  - `tools/`
  - `README.md`
- `apps/electron/`
  - placeholder host-entry skeleton only
- `docs/`
  - repository-level structure and integration documents

## Boundary Rule

- `kernel/` is the only current buildable product root.
- `apps/electron/` is not a running app yet.
- root `docs/` does not replace `kernel/docs/`.
- sealed kernel contracts, regression matrices, checklists, and status documents remain under `kernel/docs/`.
