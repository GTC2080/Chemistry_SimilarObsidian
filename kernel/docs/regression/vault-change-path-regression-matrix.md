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
- `kernel_relativize_vault_path(...)` converts host paths inside the opened
  vault root into `/`-separated vault-relative paths
- `kernel_relativize_vault_path(...)` rejects paths outside the opened vault
  root
- `kernel_relativize_vault_path(...)` rejects embedded NUL bytes, null output,
  and null handles
- `kernel_relativize_vault_path(...)` rejects the vault root when
  `allow_empty == 0`
- `kernel_relativize_vault_path(...)` accepts the vault root and returns an
  empty kernel-owned buffer when `allow_empty != 0`
- `kernel_read_vault_file(...)` reads exact raw bytes for files inside the
  opened vault root
- `kernel_read_vault_file(...)` preserves embedded NUL bytes
- `kernel_read_vault_file(...)` rejects paths outside the opened vault root
- `kernel_read_vault_file(...)` rejects null output and null handles
- `kernel_compute_pdf_file_lightweight_hash(...)` reads the PDF hash basis from
  a file inside the opened vault root
- `kernel_compute_pdf_file_lightweight_hash(...)` rejects paths outside the
  opened vault root
- `kernel_compute_pdf_file_lightweight_hash(...)` rejects null output and null
  handles
- `kernel_read_pdf_annotation_file(...)` and
  `kernel_write_pdf_annotation_file(...)` use the kernel PDF annotation storage
  key and preserve JSON bytes
- duplicate paths are removed after normalization
- first-seen order is preserved
- original path case is preserved
- `kernel_free_path_list(...)` resets output and is safe on repeat calls
- null output returns `KERNEL_ERROR_INVALID_ARGUMENT`

Tauri bridge coverage:

- watcher event classification passes changed/removed relative path candidates
  through `kernel_filter_supported_vault_paths_filtered(...)` before emitting
  `vault:fs-change`
- watcher event classification passes absolute notify event paths through
  `kernel_relativize_vault_path(...)` before supported-path filtering
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
  kernel `ai_embedding_cache` rows
- one-off note read/write and chemistry reference commands use
  `kernel_normalize_vault_relative_path(...)` instead of rebuilding relative
  path validation rules in Rust
- by-path note and vault entry commands use
  `kernel_relativize_vault_path(...)` instead of rebuilding vault-root
  `strip_prefix`, separator normalization, or parent-segment checks in Rust
- `read_binary_file` uses `kernel_read_vault_file(...)` instead of Rust
  `fs::read(...)` plus local file existence/type checks
- `read_pdf_file` uses `kernel_read_vault_file(...)` instead of Rust
  `tokio::fs::read(...)`
- `save_pdf_annotations` uses
  `kernel_compute_pdf_file_lightweight_hash(...)` instead of Rust
  `File::open(...)`, `seek(...)`, or `read(...)`
- `load_pdf_annotations` / `save_pdf_annotations` use kernel annotation JSON
  read/write surfaces instead of Rust filesystem calls
- watcher event classification uses `kernel_relativize_vault_path(...)` instead
  of rebuilding vault-root `strip_prefix` checks in Rust
