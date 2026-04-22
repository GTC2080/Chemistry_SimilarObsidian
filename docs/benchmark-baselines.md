<!-- Reason: This file freezes the benchmark datasets, baselines, and regression thresholds used for post-alpha hardening. -->

# Benchmark Baselines

Last updated: `2026-04-22`

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
  - includes attachment catalog, single attachment lookup, note attachment refs, attachment referrers, PDF source-ref/referrer, and Track 4 domain query smoke loops

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
  - `attachment_catalog_query_baseline_ms=32`
  - `attachment_catalog_query_threshold_ms=140`
  - `attachment_lookup_query_baseline_ms=18`
  - `attachment_lookup_query_threshold_ms=120`
  - `note_attachment_refs_query_baseline_ms=32`
  - `note_attachment_refs_query_threshold_ms=140`
  - `attachment_referrers_query_baseline_ms=17`
  - `attachment_referrers_query_threshold_ms=160`
  - `pdf_source_refs_query_baseline_ms=100`
  - `pdf_source_refs_query_threshold_ms=180`
  - `pdf_referrers_query_baseline_ms=162`
  - `pdf_referrers_query_threshold_ms=220`
  - `domain_attachment_metadata_query_baseline_ms=29`
  - `domain_attachment_metadata_query_threshold_ms=140`
  - `domain_pdf_metadata_query_baseline_ms=45`
  - `domain_pdf_metadata_query_threshold_ms=160`
  - `domain_attachment_objects_query_baseline_ms=25`
  - `domain_attachment_objects_query_threshold_ms=120`
  - `domain_pdf_objects_query_baseline_ms=29`
  - `domain_pdf_objects_query_threshold_ms=140`
  - `domain_object_lookup_query_baseline_ms=31`
  - `domain_object_lookup_query_threshold_ms=120`
  - `domain_note_source_refs_query_baseline_ms=117`
  - `domain_note_source_refs_query_threshold_ms=220`
  - `domain_object_referrers_query_baseline_ms=256`
  - `domain_object_referrers_query_threshold_ms=340`
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
  - `clean_observed_ms=1860`
  - `recovery_observed_ms=2707`
- io
  - `io_roundtrip_observed_ms=256`
  - `external_create_observed_ms=245`
- rebuild
  - `rebuild_observed_ms=11911`
- query
  - `tag_query_observed_ms=165`
  - `title_query_observed_ms=60`
  - `body_query_observed_ms=60`
  - `body_snippet_query_observed_ms=62`
  - `title_only_query_observed_ms=59`
  - `shallow_page_query_observed_ms=141`
  - `deep_offset_query_observed_ms=137`
  - `filtered_note_query_observed_ms=129`
  - `attachment_path_query_observed_ms=92`
  - `attachment_catalog_query_observed_ms=55`
  - `attachment_lookup_query_observed_ms=21`
  - `note_attachment_refs_query_observed_ms=33`
  - `attachment_referrers_query_observed_ms=16`
  - `pdf_source_refs_query_observed_ms=113`
  - `pdf_referrers_query_observed_ms=199`
  - `domain_attachment_metadata_query_observed_ms=27`
  - `domain_pdf_metadata_query_observed_ms=45`
  - `domain_attachment_objects_query_observed_ms=24`
  - `domain_pdf_objects_query_observed_ms=27`
  - `domain_object_lookup_query_observed_ms=30`
  - `domain_note_source_refs_query_observed_ms=124`
  - `domain_object_referrers_query_observed_ms=218`
  - `all_kind_query_observed_ms=184`
  - `ranked_title_query_observed_ms=84`
  - `ranked_tag_boost_query_observed_ms=86`
  - `ranked_all_kind_query_observed_ms=228`
  - `backlink_query_observed_ms=145`

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
