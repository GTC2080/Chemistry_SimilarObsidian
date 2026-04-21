# Build Entry Unification Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Give the repository one standard CMake configure/build/test entry point rooted at `out/build`.

**Architecture:** Check in a single `CMakePresets.json` that fixes the generator, cache variables, and binary directory, then add a root `README.md` that points humans at the same commands. Keep the change limited to repo entry conventions, not product behavior.

**Tech Stack:** CMake presets, Markdown documentation, Visual Studio 2022 generator

---

### Task 1: Add Checked-In CMake Entry Points

**Files:**
- Create: `E:\测试\Chemistry_Obsidian\CMakePresets.json`
- Modify: `E:\测试\Chemistry_Obsidian\.gitignore`
- Test: `cmake --preset dev`

- [ ] **Step 1: Define the shared configure preset**

Set the preset file to use:

- `Visual Studio 17 2022`
- `${sourceDir}/out/build`
- `KERNEL_BUILD_TESTS=ON`
- `KERNEL_BUILD_BENCHMARKS=ON`

- [ ] **Step 2: Define the shared build and test presets**

Add:

- `build-debug`
- `build-release`
- `test-debug`

- [ ] **Step 3: Ignore local user overrides**

Add `/CMakeUserPresets.json` to `.gitignore`.

- [ ] **Step 4: Verify the preset file loads**

Run: `cmake --preset dev`
Expected: CMake configures into `out/build` without creating a root `build/` directory.

### Task 2: Add the Canonical Human Entry Point

**Files:**
- Create: `E:\测试\Chemistry_Obsidian\README.md`
- Test: read the README commands against `CMakePresets.json`

- [ ] **Step 1: Document prerequisites**

State that contributors should use a shell where `cmake` and `ctest` are on `PATH`.

- [ ] **Step 2: Document the standard command sequence**

Document:

- `cmake --preset dev`
- `cmake --build --preset build-debug`
- `ctest --preset test-debug`
- `cmake --build --preset build-release`

- [ ] **Step 3: Document the output layout**

State that:

- the build tree is `out/build`
- tests live under `out/build/tests/<Config>/`
- benchmarks live under `out/build/benchmarks/<Config>/`

- [ ] **Step 4: Verify the README matches the preset names**

Expected: every command in the README maps directly to a checked-in preset or documented target path.
