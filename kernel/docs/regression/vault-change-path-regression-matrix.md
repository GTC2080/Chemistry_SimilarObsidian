<!-- Reason: This file records regression obligations for kernel-owned changed-entry path normalization. -->

# Vault Change Path Regression Matrix

Required coverage:

- `kernel_filter_changed_markdown_paths(...)` trims path whitespace
- backslashes are normalized to `/`
- non-Markdown paths are removed
- `.MD` and other case variants are accepted
- duplicate paths are removed after normalization
- first-seen order is preserved
- original path case is preserved
- `kernel_free_path_list(...)` resets output and is safe on repeat calls
- null output returns `KERNEL_ERROR_INVALID_ARGUMENT`

Tauri bridge coverage:

- `scan_changed_entries` uses kernel-filtered relative paths before querying the
  kernel note catalog
- `index_changed_entries` uses kernel-filtered relative paths before reading
  note content from the kernel
- `read_note_indexed_content` uses kernel-filtered relative paths before
  reading note content from the kernel
- `remove_deleted_entries` uses kernel-filtered relative paths before deleting
  legacy embedding cache rows
