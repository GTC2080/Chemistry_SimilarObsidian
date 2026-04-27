<!-- Reason: This file freezes the kernel-owned changed-entry path normalization surface used by Tauri incremental scans. -->

# Vault Change Path Contract

## Scope

Current surface:

- `kernel_filter_changed_markdown_paths(changed_paths_lf, out_paths)`
- `kernel_filter_supported_vault_paths(changed_paths_lf, out_paths)`
- `kernel_filter_supported_vault_paths_filtered(changed_paths_lf, ignored_roots_csv, out_paths)`
- `kernel_free_path_list(out_paths)`

The input is a line-feed separated list of changed relative paths from the host
watcher or frontend. The kernel returns canonical subsets that should drive
incremental vault event fanout and Markdown note scan/index work.

## Ownership

- the kernel owns returned path strings and the path array
- callers release the list with `kernel_free_path_list(...)`
- `kernel_free_path_list(...)` is idempotent and leaves `paths == nullptr` and
  `count == 0`
- null output returns `KERNEL_ERROR_INVALID_ARGUMENT`

## Frozen Semantics

All path filters:

- trim leading and trailing whitespace from each input path
- normalize path separators to `/`
- drop empty paths
- preserve original path case
- deduplicate after normalization while preserving first-seen order

`kernel_filter_changed_markdown_paths(...)`:

- keep only paths whose final filename extension is `.md`, case-insensitive

`kernel_filter_supported_vault_paths(...)`:

- keep only paths whose final filename extension is in the vault event support
  set, case-insensitive
- the vault event support set is:
  - `md`, `txt`, `json`, `py`, `rs`, `js`, `ts`, `jsx`, `tsx`, `css`, `html`
  - `toml`, `yaml`, `yml`, `xml`, `sh`, `bat`, `c`, `cpp`, `h`, `java`, `go`
  - `png`, `jpg`, `jpeg`, `gif`, `svg`, `webp`, `bmp`, `ico`, `pdf`
  - `mol`, `chemdraw`, `paper`, `csv`, `jdx`, `pdb`, `xyz`, `cif`

`kernel_filter_supported_vault_paths_filtered(...)`:

- applies all `kernel_filter_supported_vault_paths(...)` semantics
- accepts a comma-separated ignored root list
- trims ignored roots, strips leading/trailing slashes, normalizes `\` to `/`,
  and compares only the first root segment
- drops paths whose first relative path segment matches an ignored root exactly
- drops any path with a hidden segment whose name starts with `.`

## Host Boundary

Tauri Rust may still:

- receive file-change vectors from the watcher/front-end command payload
- pass the vectors through the sealed kernel bridge
- pass raw ignored-root CSV through the sealed kernel bridge
- use returned relative paths to request kernel note catalog/read surfaces
- use returned relative paths to emit supported vault file-change events
- use the same kernel path filter to validate one-off indexed Markdown note
  reads
- use the same kernel path filter before deleting legacy embedding cache rows
  for removed Markdown notes
- ignore directory events as a platform watcher concern before calling the
  kernel path filter

Tauri Rust must not reimplement supported-extension filtering, changed-entry
Markdown filtering, hidden segment filtering, ignored-root filtering,
ignored-root normalization, or path deduplication.
