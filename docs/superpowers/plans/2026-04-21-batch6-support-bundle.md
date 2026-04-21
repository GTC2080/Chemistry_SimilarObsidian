# Batch 6 Support Bundle Implementation Plan

Status: `Completed on 2026-04-21`

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Extend diagnostics into a support-oriented single-file bundle, add stable rebuild duration observability, and freeze one repository-local release checklist for Batch 6.

**Architecture:** Reuse the current runtime snapshots instead of inventing a new subsystem. Add a bounded recent-events ring to runtime state, thread those events into diagnostics JSON, extend the rebuild snapshot with duration, and document the official release path around `kernel_phase_gate` plus existing checklists.

**Tech Stack:** C++20, MSVC, existing diagnostics JSON export, existing rebuild runtime, CMake custom targets, Markdown repository docs

---

### Task 1: Lock Support Bundle Expectations in Tests

**Files:**
- Modify: `tests/api/kernel_api_tests.cpp`

- [x] Add a diagnostics regression for `recent_events` stable ordering.
- [x] Add a diagnostics regression for `logger_backend`.
- [x] Extend rebuild diagnostics tests to assert `last_rebuild_duration_ms`.
- [x] Build `kernel_api_tests` and confirm the new checks fail for missing behavior.

### Task 2: Add Runtime Recent Events and Rebuild Duration

**Files:**
- Modify: `src/internal/core/kernel_internal.h`
- Modify: `src/internal/core/kernel_shared.h`
- Modify: `src/impl/core/kernel_shared.cpp`
- Modify: `src/impl/core/kernel_runtime.cpp`
- Modify: `src/impl/core/kernel_rebuild.cpp`
- Modify: `src/impl/core/kernel_watcher_runtime.cpp`

- [x] Add a bounded recent-events runtime structure.
- [x] Add helpers to append runtime events in stable order.
- [x] Record startup recovery outcome, live fault events, continuity fallback, rebuild start/result, and watcher recovery events.
- [x] Extend the rebuild snapshot with duration in milliseconds.

### Task 3: Export the Support Bundle Through Diagnostics

**Files:**
- Modify: `src/impl/core/kernel_diagnostics.cpp`

- [x] Export `recent_events`.
- [x] Export `logger_backend`.
- [x] Export `last_rebuild_duration_ms`.
- [x] Keep diagnostics output single-file and deterministic.
- [x] Re-run `kernel_api_tests` and confirm the new diagnostics checks pass.

### Task 4: Freeze the Release Checklist

**Files:**
- Create: `docs/release-checklist.md`
- Modify: `docs/kernel-phase1-status.md`
- Modify: `docs/benchmark-baselines.md`

- [x] Write one explicit release checklist with exact commands and pass criteria.
- [x] Link the checklist from the existing status/baseline docs.
- [x] Keep the checklist grounded in existing gates rather than adding a new one.

### Task 5: Final Verification

**Files:**
- Modify as needed based on verification failures

- [x] Run `cmake --build out/build --config Debug --target kernel_api_tests`.
- [x] Run `E:/测试/Chemistry_Obsidian/out/build/tests/Debug/kernel_api_tests.exe`.
- [x] Run `cmake --build out/build --config Debug --target kernel_phase_gate`.
- [x] Confirm all diagnostics, rebuild, and checklist changes are reflected in docs before closing Batch 6.
