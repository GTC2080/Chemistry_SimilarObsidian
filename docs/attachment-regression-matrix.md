<!-- Reason: This file maps the Track 2 Batch 1 attachment public surface contract to the repository regression suite so later attachment work cannot silently drift host-visible behavior. -->

# Attachment Regression Matrix

Last updated: `2026-04-21`

## Scope

This matrix covers the formal Batch 1 attachment public surface:

- `kernel_query_attachments(...)`
- `kernel_get_attachment(...)`
- `kernel_query_note_attachment_refs(...)`
- `kernel_query_attachment_referrers(...)`
- attachment/search live-catalog consistency

## Contract To Regression Mapping

### Live Catalog Visibility

- live present attachments are visible in the catalog
  - `test_attachment_public_surface_lists_live_catalog_and_single_attachment`
- live missing attachments remain visible in the catalog
  - `test_attachment_public_surface_lists_live_catalog_and_single_attachment`
- unreferenced disk files are excluded from the catalog
  - `test_attachment_public_surface_lists_live_catalog_and_single_attachment`
- orphaned metadata rows are excluded from the catalog
  - `test_attachment_public_surface_excludes_orphaned_paths_and_matches_search`

### Single Attachment Lookup

- live present lookup succeeds
  - `test_attachment_public_surface_lists_live_catalog_and_single_attachment`
- live missing lookup succeeds
  - `test_attachment_public_surface_lists_live_catalog_and_single_attachment`
- unreferenced disk file lookup returns `NOT_FOUND`
  - `test_attachment_public_surface_lists_live_catalog_and_single_attachment`
- orphaned metadata row lookup returns `NOT_FOUND`
  - `test_attachment_public_surface_excludes_orphaned_paths_and_matches_search`

### Metadata Contract

- present attachments expose non-zero `file_size` and `mtime_ns`
  - `test_attachment_public_surface_lists_live_catalog_and_single_attachment`
- never-observed missing attachments may expose `file_size=0` and `mtime_ns=0`
  - `test_attachment_public_surface_lists_live_catalog_and_single_attachment`
- generic, chem-like, and extensionless attachments map to stable coarse kinds
  - `test_attachment_public_surface_metadata_contract_covers_kind_mapping_and_missing_carry_forward`
- watcher-driven delete preserves last reconciled `file_size` and `mtime_ns` while flipping `presence` to `missing`
  - `test_attachment_public_surface_metadata_contract_covers_kind_mapping_and_missing_carry_forward`

### Lifecycle Consistency

- note rewrite replaces the live attachment ref set in the formal public surface
  - `test_attachment_api_rewrite_recovery_and_rebuild_follow_live_state`
- attachment-only rename keeps the old live ref path visible as `missing` and excludes the unreferenced renamed path from the formal public surface
  - `test_attachment_api_observes_attachment_rename_reconciliation`
- rebuild restores the formal live attachment ref set after stale attachment-ref drift
  - `test_attachment_api_rewrite_recovery_and_rebuild_follow_live_state`
- startup recovery restores recovered missing attachment refs through the formal public surface
  - `test_attachment_api_rewrite_recovery_and_rebuild_follow_live_state`
- startup recovery replaces stale attachment refs and exposes recovered present metadata through the formal public surface
  - `test_startup_recovery_replaces_stale_attachment_refs_and_metadata`
- startup recovery preserves recovered missing attachment refs and missing metadata through the formal public surface
  - `test_startup_recovery_marks_missing_attachments_for_recovered_note_refs`
- reopen catch-up preserves the live attachment ref while reconciling a closed-window delete to `missing`
  - `test_reopen_catch_up_repairs_attachment_missing_state_after_closed_window_delete`
- reopen catch-up reconciles a watcher-backoff attachment create back to `present` through the formal public surface
  - `test_close_during_watcher_fault_backoff_leaves_attachment_create_for_reopen_catch_up`
- reopen catch-up reconciles a watcher-backoff attachment delete back to `missing` through the formal public surface
  - `test_close_during_watcher_fault_backoff_leaves_attachment_delete_for_reopen_catch_up`
- reopen catch-up refreshes watcher-backoff attachment modify metadata through the formal public surface
  - `test_close_during_watcher_fault_backoff_leaves_attachment_modify_for_reopen_catch_up`
- rebuild preserves the live attachment ref while reconciling a deleted file to `missing`
  - `test_rebuild_reconciles_attachment_missing_state`

### Note -> Attachment Refs

- note refs return live attachment records
  - `test_attachment_public_surface_note_refs_and_referrers_are_stable`
- live notes without attachment refs return an empty successful result
  - `test_attachment_public_surface_note_refs_and_referrers_are_stable`
- note refs preserve persisted ref order
  - `test_attachment_public_surface_note_refs_and_referrers_are_stable`
- note refs expose global live `ref_count`
  - `test_attachment_public_surface_note_refs_and_referrers_are_stable`
- deleted notes return `NOT_FOUND` from the formal note attachment refs surface
  - `test_startup_recovery_plus_reopen_catch_up_removes_deleted_note_drift`

### Attachment -> Referrers

- attachment referrers return live note referrers only
  - `test_attachment_public_surface_note_refs_and_referrers_are_stable`
- attachment referrers sort by `note_rel_path`
  - `test_attachment_public_surface_note_refs_and_referrers_are_stable`
- orphaned attachment referrers return `NOT_FOUND`
  - `test_attachment_public_surface_excludes_orphaned_paths_and_matches_search`
- attachments owned only by deleted notes return `NOT_FOUND` from the formal referrers surface
  - `test_startup_recovery_plus_reopen_catch_up_removes_deleted_note_drift`

### Search Consistency

- attachment path search excludes orphaned paths
  - `test_attachment_public_surface_excludes_orphaned_paths_and_matches_search`
- attachment path search agrees with the public live catalog
  - `test_attachment_public_surface_excludes_orphaned_paths_and_matches_search`
- attachment path search keeps the old live ref path after attachment-only rename and excludes the unreferenced renamed path
  - `test_attachment_api_observes_attachment_rename_reconciliation`

### Benchmark Gate

- query benchmark exercises the attachment live catalog list surface
  - `kernel_query_benchmark.exe`
- query benchmark exercises single attachment lookup
  - `kernel_query_benchmark.exe`
- query benchmark exercises note -> attachment refs
  - `kernel_query_benchmark.exe`
- query benchmark exercises attachment -> referrers
  - `kernel_query_benchmark.exe`

### Diagnostics Export

- support bundle exports the attachment public surface revision
  - `test_export_diagnostics_writes_json_snapshot`
- support bundle exports the attachment metadata contract revision
  - `test_export_diagnostics_writes_json_snapshot`
- support bundle exports the attachment kind mapping revision
  - `test_export_diagnostics_writes_json_snapshot`
- support bundle exports live attachment count
  - `test_export_diagnostics_writes_json_snapshot`
- support bundle exports orphaned attachment count
  - `test_export_diagnostics_writes_json_snapshot`
