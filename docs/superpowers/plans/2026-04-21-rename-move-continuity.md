# Rename/Move Continuity Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Stabilize Phase 1 rename/move continuity by keeping explicit continuity only for clearly recognized watcher rename pairs, making ambiguous cases degrade predictably, and exporting the latest continuity fallback reason through diagnostics.

**Architecture:** Keep the continuity decision in the watcher coalescer. Extend coalesced watcher actions with internal-only fallback metadata so recognized rename/move paths can still flow through `RenamePath`, while degraded cases carry a reason string that the watcher runtime records into runtime state for diagnostics export. Do not widen the public C ABI.

**Tech Stack:** C++20, MSVC, Windows watcher session/coalescer, existing kernel runtime/diagnostics JSON export, custom test executables

---

### Task 1: Lock Continuity/Fallback Behavior in Tests

**Files:**
- Modify: `tests/watcher/watcher_tests.cpp`
- Modify: `tests/api/kernel_api_tests.cpp`

- [ ] Add watcher-unit coverage for explicit continuity and degraded rename/move cases.
- [ ] Add an API diagnostics regression proving the latest continuity fallback reason is exported.
- [ ] Build the targeted test binary and verify the new tests fail for the expected missing behavior.

### Task 2: Carry Fallback Metadata Through Watcher Actions

**Files:**
- Modify: `src/internal/watcher/watcher.h`
- Modify: `src/impl/watcher/watcher.cpp`

- [ ] Extend the internal coalesced-action model with optional continuity fallback metadata.
- [ ] Tighten rename pairing so clearly recognized rename/move paths still produce `RenamePath`.
- [ ] Mark degraded rename-old / rename-new cases with stable fallback reasons instead of silent generic delete/refresh behavior.
- [ ] Re-run watcher tests and verify the new coalescer behavior passes.

### Task 3: Export Continuity Fallback Through Diagnostics

**Files:**
- Modify: `src/internal/core/kernel_internal.h`
- Modify: `src/internal/core/kernel_shared.h`
- Modify: `src/impl/core/kernel_shared.cpp`
- Modify: `src/impl/core/kernel_watcher_runtime.cpp`
- Modify: `src/impl/core/kernel_diagnostics.cpp`

- [ ] Add runtime state for the latest continuity fallback reason and timestamp.
- [ ] Record fallback metadata when the watcher runtime consumes degraded continuity actions.
- [ ] Export the latest fallback reason/timestamp in diagnostics JSON.
- [ ] Re-run the targeted API test and verify it passes.

### Task 4: Close Docs and Full Verification

**Files:**
- Modify: `docs/kernel-phase1-status.md`
- Modify: `docs/watcher-lifecycle-checklist.md`

- [ ] Freeze the new continuity/fallback semantics in repo docs.
- [ ] Run `cmake --build out/build --config Debug --target watcher_tests`.
- [ ] Run `E:/测试/Chemistry_Obsidian/out/build/tests/Debug/watcher_tests.exe`.
- [ ] Run `cmake --build out/build --config Debug --target kernel_phase_gate`.
- [ ] Confirm the full gate passes before claiming the batch step is complete.
