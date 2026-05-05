# Repo Structure

## Purpose

Nexus · Scientist Obsidian uses a two-layer structure:

- `kernel/`
  - the sealed kernel platform and its full validation surface
- `apps/`
  - application entrypoints built on top of the sealed kernel

## Current Tree

- `README.md`
  - Chinese project entrypoint for Nexus · Scientist Obsidian
- `README_EN.md`
  - English project entrypoint for Nexus · Scientist Obsidian
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
- `apps/tauri/`
  - current Tauri desktop host, React frontend, and Rust bridge
- `docs/`
  - repository-level structure and integration documents

## Boundary Rule

- root `README.md` and `README_EN.md` are the only README files in the project source.
- `kernel/` is the sealed truth and compute root.
- `apps/tauri/` is the current running host app.
- root `docs/` does not replace `kernel/docs/`.
- sealed kernel contracts, regression matrices, checklists, and status documents remain under `kernel/docs/`.
