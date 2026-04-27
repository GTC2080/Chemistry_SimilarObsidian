<!-- Reason: This file freezes the kernel-owned note catalog surface consumed by Tauri scan and indexing commands. -->

# Note Catalog Contract

## Scope

Current surface:

- `kernel_get_note_catalog_default_limit(out_limit)`
- `kernel_query_notes(handle, limit, out_notes)`
- `kernel_query_notes_filtered(handle, limit, ignored_roots_csv, out_notes)`
- `kernel_free_note_list(out_notes)`

The note catalog is the host-facing metadata surface for active Markdown notes.

## Ownership

- the kernel owns returned note records and strings
- the kernel owns the default host-facing catalog scan limit returned by
  `kernel_get_note_catalog_default_limit(...)`
- callers release the list with `kernel_free_note_list(...)`
- `kernel_free_note_list(...)` is idempotent and leaves `notes == nullptr` and
  `count == 0`
- invalid handle, null output, or zero limit returns `KERNEL_ERROR_INVALID_ARGUMENT`

## Frozen Semantics

- catalog rows are live Markdown notes from kernel storage
- hosts that need the default scan/index catalog limit must read it from
  `kernel_get_note_catalog_default_limit(...)`, not from a Rust constant
- rows are sorted by relative path
- returned metadata includes relative path, parser title, file size, mtime, and
  content revision
- the limit applies before ignored-root filtering
- filtered queries parse comma-separated ignored root names by trimming
  whitespace and leading/trailing slashes
- filtered queries remove only notes whose first relative-path segment exactly
  matches an ignored root
- root files with similar names remain visible unless the whole first segment
  matches an ignored root

## Host Boundary

Tauri Rust may still attach host-specific absolute paths for existing frontend
DTOs and orchestrate AI embedding compatibility writes.

Tauri Rust must not rebuild note catalog truth, ignored-root filtering, or
embeddable-note extension truth from raw note rows. It must also not retain a
duplicate default scan/index catalog limit.
