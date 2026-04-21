<!-- Reason: This file freezes the benchmark datasets, baselines, and regression thresholds used for post-alpha hardening. -->

# Benchmark Baselines

Last updated: `2026-04-21`

## Purpose

This file freezes the current benchmark baselines and regression thresholds.
Benchmark executables now enforce these gates directly:

- if `observed_ms <= threshold_ms`, the benchmark passes
- if `observed_ms > threshold_ms`, the benchmark exits non-zero

The repository now exposes named build targets for this verification:

- `kernel_benchmark_gates`
  - runs only the benchmark regression gates
- `kernel_phase_gate`
  - runs `ctest`
  - then runs all benchmark regression gates

The full release flow is frozen in:

- [release-checklist.md](/E:/测试/Chemistry_Obsidian/docs/release-checklist.md)

The goal is not micro-optimization.
The goal is to keep Phase 1 post-alpha hardening from silently regressing startup, watcher, rebuild, or query paths.

## Fixed Datasets

- `startup_benchmark`
  - clean open/close loop: `clean_iterations=100`
  - recovery open/close loop: `recovery_iterations=25`
- `io_benchmark`
  - write/read roundtrip loop: `iterations=100`
  - watcher-driven external create loop: `external_create_iterations=25`
- `rebuild_benchmark`
  - `note_count=64`
  - `attachment_count=64`
  - `rebuild_iterations=25`
- `query_benchmark`
  - `note_count=64`
  - `filter_note_count=16`
  - `iterations=200`

## Current Gates

- startup
  - `clean_baseline_ms=3465`
  - `clean_threshold_ms=6000`
  - `recovery_baseline_ms=3865`
  - `recovery_threshold_ms=7000`
- io
  - `io_roundtrip_baseline_ms=168`
  - `io_roundtrip_threshold_ms=500`
  - `external_create_baseline_ms=289`
  - `external_create_threshold_ms=800`
- rebuild
  - `rebuild_baseline_ms=15885`
  - `rebuild_threshold_ms=22000`
- query
  - `tag_query_baseline_ms=143`
  - `tag_query_threshold_ms=250`
  - `title_query_baseline_ms=88`
  - `title_query_threshold_ms=140`
  - `body_query_baseline_ms=77`
  - `body_query_threshold_ms=140`
  - `body_snippet_query_baseline_ms=66`
  - `body_snippet_query_threshold_ms=140`
  - `title_only_query_baseline_ms=61`
  - `title_only_query_threshold_ms=140`
  - `shallow_page_query_baseline_ms=139`
  - `shallow_page_query_threshold_ms=220`
  - `deep_offset_query_baseline_ms=123`
  - `deep_offset_query_threshold_ms=180`
  - `filtered_note_query_baseline_ms=124`
  - `filtered_note_query_threshold_ms=180`
  - `attachment_path_query_baseline_ms=57`
  - `attachment_path_query_threshold_ms=160`
  - `all_kind_query_baseline_ms=167`
  - `all_kind_query_threshold_ms=240`
  - `ranked_title_query_baseline_ms=51`
  - `ranked_title_query_threshold_ms=180`
  - `ranked_tag_boost_query_baseline_ms=51`
  - `ranked_tag_boost_query_threshold_ms=180`
  - `ranked_all_kind_query_baseline_ms=225`
  - `ranked_all_kind_query_threshold_ms=300`
  - `backlink_query_baseline_ms=107`
  - `backlink_query_threshold_ms=250`

## Fresh Verification Snapshot

- startup
  - `clean_observed_ms=1856`
  - `recovery_observed_ms=2034`
- io
  - `io_roundtrip_observed_ms=249`
  - `external_create_observed_ms=325`
- rebuild
  - `rebuild_observed_ms=11910`
- query
  - `tag_query_observed_ms=160`
  - `title_query_observed_ms=97`
  - `body_query_observed_ms=123`
  - `body_snippet_query_observed_ms=86`
  - `title_only_query_observed_ms=63`
  - `shallow_page_query_observed_ms=161`
  - `deep_offset_query_observed_ms=173`
  - `filtered_note_query_observed_ms=152`
  - `attachment_path_query_observed_ms=87`
  - `all_kind_query_observed_ms=192`
  - `ranked_title_query_observed_ms=72`
  - `ranked_tag_boost_query_observed_ms=57`
  - `ranked_all_kind_query_observed_ms=253`
  - `backlink_query_observed_ms=95`

## Update Rule

Only update these baselines after:

1. a full Debug build is green
2. `kernel_phase_gate` passes
3. all benchmark executables pass their current gates
4. a deliberate decision is made to accept a new performance profile

## Standard Commands

- full phase gate
  - `cmake --build E:\测试\Chemistry_Obsidian\out\build --config Debug --target kernel_phase_gate`
- benchmark-only gate
  - `cmake --build E:\测试\Chemistry_Obsidian\out\build --config Debug --target kernel_benchmark_gates`
