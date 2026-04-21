# Build Entry Design

## Goal

Standardize repository setup, build, and test entry points so contributors always use the same out-of-source tree and do not recreate a top-level `build/` directory.

## Decision

The repository will treat `CMakePresets.json` as the canonical machine-readable entry point and `README.md` as the canonical human-readable entry point.

- `out/build` is the only supported shared build tree.
- `cmake --preset dev` is the standard configure command.
- `cmake --build --preset build-debug` and `ctest --preset test-debug` are the standard verification commands.
- A top-level `build/` directory is considered an accidental local artifact, not a supported workflow.

## Why This Shape

Presets let command-line users, IDEs, and automation resolve the same generator, cache variables, and build directory without copying ad-hoc commands. A root README gives future contributors one obvious place to start instead of hunting through dated plan documents.

## Scope

This change only standardizes repository entry points. It does not change product code, CMake target structure, or test behavior.
