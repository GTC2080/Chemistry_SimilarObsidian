<!-- Reason: This file freezes the Phase 1 release checklist so release readiness is determined by repository-local commands and pass conditions instead of memory. -->

# Release Checklist

Last updated: `2026-04-21`

## Purpose

This checklist is the official release-ready entrypoint for the current Phase 1 kernel line.
It does not add new gates.
It names the existing commands, checklists, and pass conditions that must be satisfied before calling the line stable.

## Required Commands

1. Full diagnostics smoke and API regression

- Build:
  - `cmake --build E:\测试\Chemistry_Obsidian\out\build --config Debug --target kernel_api_tests`
- Run:
  - `E:\测试\Chemistry_Obsidian\out\build\tests\Debug\kernel_api_tests.exe`
- Pass condition:
  - exit code `0`
  - diagnostics export regressions pass, including support-bundle fields, rebuild observability, and runtime-state snapshots

2. Full Phase 1 gate

- Run:
  - `cmake --build E:\测试\Chemistry_Obsidian\out\build --config Debug --target kernel_phase_gate`
- Pass condition:
  - `ctest` reports zero failed tests
  - all benchmark executables pass their built-in thresholds

## Required Checklist Reviews

Before release, confirm these repository documents still match the shipped behavior:

- [recovery-crash-matrix-checklist.md](/E:/测试/Chemistry_Obsidian/docs/recovery-crash-matrix-checklist.md)
- [watcher-lifecycle-checklist.md](/E:/测试/Chemistry_Obsidian/docs/watcher-lifecycle-checklist.md)
- [attachment-query-contract.md](/E:/测试/Chemistry_Obsidian/docs/attachment-query-contract.md)
- [attachment-metadata-contract.md](/E:/测试/Chemistry_Obsidian/docs/attachment-metadata-contract.md)
- [attachment-regression-matrix.md](/E:/测试/Chemistry_Obsidian/docs/attachment-regression-matrix.md)
- [search-query-contract.md](/E:/测试/Chemistry_Obsidian/docs/search-query-contract.md)
- [search-regression-matrix.md](/E:/测试/Chemistry_Obsidian/docs/search-regression-matrix.md)
- [benchmark-baselines.md](/E:/测试/Chemistry_Obsidian/docs/benchmark-baselines.md)

## Baseline Update Rule

Do not update performance baselines unless all of the following are true:

1. Debug build is green.
2. `kernel_phase_gate` passes.
3. benchmark executables pass the current thresholds first.
4. the new performance profile is intentionally accepted and documented.

## Diagnostics Smoke Definition

Diagnostics smoke is considered satisfied when `kernel_api_tests.exe` passes the diagnostics export regressions that cover:

- healthy snapshot export
- attachment contract export
- search contract export
- search pagination export
- search filter export
- search ranking export
- startup recovery outcome export
- watcher fault and recovery export
- rebuild in-flight and completed-result export
- support-bundle recent-events export
- latest continuity fallback export

## Release Decision

Release readiness is true only when:

- required commands are green
- required checklist reviews still match current behavior
- no open behavior change remains undocumented in the repository
