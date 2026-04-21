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

### Note -> Attachment Refs

- note refs return live attachment records
  - `test_attachment_public_surface_note_refs_and_referrers_are_stable`
- note refs preserve persisted ref order
  - `test_attachment_public_surface_note_refs_and_referrers_are_stable`
- note refs expose global live `ref_count`
  - `test_attachment_public_surface_note_refs_and_referrers_are_stable`

### Attachment -> Referrers

- attachment referrers return live note referrers only
  - `test_attachment_public_surface_note_refs_and_referrers_are_stable`
- attachment referrers sort by `note_rel_path`
  - `test_attachment_public_surface_note_refs_and_referrers_are_stable`
- orphaned attachment referrers return `NOT_FOUND`
  - `test_attachment_public_surface_excludes_orphaned_paths_and_matches_search`

### Search Consistency

- attachment path search excludes orphaned paths
  - `test_attachment_public_surface_excludes_orphaned_paths_and_matches_search`
- attachment path search agrees with the public live catalog
  - `test_attachment_public_surface_excludes_orphaned_paths_and_matches_search`

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
- support bundle exports the attachment kind mapping revision
  - `test_export_diagnostics_writes_json_snapshot`
- support bundle exports live attachment count
  - `test_export_diagnostics_writes_json_snapshot`
