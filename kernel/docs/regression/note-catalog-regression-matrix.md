<!-- Reason: This file records regression obligations for the kernel-owned note catalog surface. -->

# Note Catalog Regression Matrix

Required coverage:

- `kernel_get_note_catalog_default_limit(...)` returns the kernel-owned default
  scan/index catalog limit and rejects null output
- `kernel_get_vault_scan_default_limit(...)` returns the kernel-owned fast vault
  metadata scan limit and rejects null output
- `kernel_query_notes(...)` returns sorted live note metadata
- note rows include rel path, parser title, file size, mtime, and content revision
- limit caps source catalog rows
- `kernel_query_notes_filtered(...)` removes notes below exactly matching
  ignored top-level roots
- filtered note catalog queries trim whitespace and leading/trailing slashes from
  ignored root names
- filtered note catalog queries keep root files with names similar to ignored
  folder names
- `kernel_free_note_list(...)` resets output
- null handle returns `KERNEL_ERROR_INVALID_ARGUMENT`
- zero limit returns `KERNEL_ERROR_INVALID_ARGUMENT`
- null output returns `KERNEL_ERROR_INVALID_ARGUMENT`

Tauri bridge coverage:

- `scan_vault` consumes `kernel_query_notes_filtered(...)`
- `scan_vault` reads its fast metadata scan limit through the sealed kernel
  bridge instead of a duplicate Rust constant
- `index_vault_content` obtains embedding refresh jobs from the kernel instead
  of consuming note catalog rows and reading note content in Rust
- scan/index catalog queries read their default limit through the sealed kernel
  bridge instead of a duplicate Rust constant
- AI embedding refresh treats kernel note catalog rows as the Markdown
  embeddable note surface through the kernel refresh job planner instead of
  applying a duplicate Rust extension gate
