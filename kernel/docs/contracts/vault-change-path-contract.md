<!-- Reason: This file freezes the kernel-owned changed-entry path normalization surface used by Tauri incremental scans. -->

# Vault Change Path Contract

## Scope

Current surface:

- `kernel_filter_changed_markdown_paths(changed_paths_lf, out_paths)`
- `kernel_free_path_list(out_paths)`

The input is a line-feed separated list of changed relative paths from the host
watcher or frontend. The kernel returns the canonical subset that should drive
incremental Markdown note scan/index work.

## Ownership

- the kernel owns returned path strings and the path array
- callers release the list with `kernel_free_path_list(...)`
- `kernel_free_path_list(...)` is idempotent and leaves `paths == nullptr` and
  `count == 0`
- null output returns `KERNEL_ERROR_INVALID_ARGUMENT`

## Frozen Semantics

- trim leading and trailing whitespace from each input path
- normalize path separators to `/`
- drop empty paths
- keep only paths whose final filename extension is `.md`, case-insensitive
- preserve original path case
- deduplicate after normalization while preserving first-seen order

## Host Boundary

Tauri Rust may still:

- receive file-change vectors from the watcher/front-end command payload
- pass the vectors through the sealed kernel bridge
- use returned relative paths to request kernel note catalog/read surfaces
- use the same kernel path filter to validate one-off indexed Markdown note
  reads
- use the same kernel path filter before deleting legacy embedding cache rows
  for removed Markdown notes

Tauri Rust must not reimplement changed-entry Markdown filtering,
normalization, or deduplication.
