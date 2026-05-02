<!-- Reason: This file records regression obligations for kernel-owned changed-entry path normalization. -->

# Vault Change Path Regression Matrix

Required coverage:

- `kernel_filter_changed_markdown_paths(...)` trims path whitespace
- `kernel_filter_supported_vault_paths(...)` trims path whitespace
- backslashes are normalized to `/`
- non-Markdown paths are removed from the Markdown scan/index filter
- unsupported vault event paths are removed from the watcher event filter
- `.MD` and other case variants are accepted
- `.PDB` and other supported non-Markdown case variants are accepted by the
  watcher event filter
- `kernel_filter_supported_vault_paths_filtered(...)` removes hidden root
  folders, hidden child folders/files, and ignored root folders
- `kernel_normalize_vault_relative_path(...)` trims path whitespace and
  normalizes backslashes
- `kernel_normalize_vault_relative_path(...)` rejects embedded NUL bytes,
  rooted paths, drive-qualified paths, empty segments, `.`, and `..`
- `kernel_normalize_vault_relative_path(...)` returns kernel-owned text released
  with `kernel_free_buffer(...)`
- duplicate paths are removed after normalization
- first-seen order is preserved
- original path case is preserved
- `kernel_free_path_list(...)` resets output and is safe on repeat calls
- null output returns `KERNEL_ERROR_INVALID_ARGUMENT`

Tauri bridge coverage:

- watcher event classification passes changed/removed relative path candidates
  through `kernel_filter_supported_vault_paths_filtered(...)` before emitting
  `vault:fs-change`
- watcher Rust only performs platform directory-event checks before kernel path
  filtering
- watcher Rust passes raw ignored-root CSV to the kernel rather than
  normalizing/deduplicating ignored roots itself
- `scan_changed_entries` uses kernel-filtered relative paths before querying the
  kernel note catalog
- `index_changed_entries` uses kernel-filtered relative paths before reading
  note content from the kernel
- `read_note_indexed_content` uses kernel-filtered relative paths before
  reading note content from the kernel
- `ask_vault` uses kernel-filtered relative paths before reading RAG note
  content from the kernel
- `remove_deleted_entries` uses kernel-filtered relative paths before deleting
  legacy embedding cache rows
- one-off note read/write and chemistry reference commands use
  `kernel_normalize_vault_relative_path(...)` instead of rebuilding relative
  path validation rules in Rust
