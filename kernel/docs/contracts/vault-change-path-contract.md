<!-- Reason: This file freezes the kernel-owned changed-entry path normalization surface used by Tauri incremental scans. -->

# Vault Change Path Contract

## Scope

Current surface:

- `kernel_filter_changed_markdown_paths(changed_paths_lf, out_paths)`
- `kernel_filter_supported_vault_paths(changed_paths_lf, out_paths)`
- `kernel_filter_supported_vault_paths_filtered(changed_paths_lf, ignored_roots_csv, out_paths)`
- `kernel_normalize_vault_relative_path(rel_path, rel_path_size, out_buffer)`
- `kernel_relativize_vault_path(handle, host_path, host_path_size, allow_empty, out_buffer)`
- `kernel_read_vault_file(handle, host_path, host_path_size, out_buffer)`
- `kernel_free_path_list(out_paths)`
- `kernel_free_buffer(out_buffer)`

The input is a line-feed separated list of changed relative paths from the host
watcher or frontend. The kernel returns canonical subsets that should drive
incremental vault event fanout and Markdown note scan/index work.

The host-path relativization surface accepts one absolute host path plus an
opened vault kernel handle, then returns the canonical vault-relative path that
may be used by by-path note, entry, and watcher event commands.
The vault file read surface accepts one absolute host path plus an opened vault
kernel handle, applies the same vault-root boundary, and returns raw file bytes
for host media previews.

## Ownership

- the kernel owns returned path strings and the path array
- callers release the list with `kernel_free_path_list(...)`
- `kernel_free_path_list(...)` is idempotent and leaves `paths == nullptr` and
  `count == 0`
- the kernel owns normalized relative path text returned by
  `kernel_normalize_vault_relative_path(...)`
- callers release normalized relative path text with `kernel_free_buffer(...)`
- the kernel owns relativized relative path text returned by
  `kernel_relativize_vault_path(...)`
- callers release relativized relative path text with `kernel_free_buffer(...)`
- the kernel owns file bytes returned by `kernel_read_vault_file(...)`
- callers release file bytes with `kernel_free_buffer(...)`
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

`kernel_normalize_vault_relative_path(...)`:

- trims leading and trailing whitespace
- normalizes path separators to `/`
- rejects empty paths
- rejects rooted paths and drive-qualified paths such as `C:/vault/note.md`
- rejects embedded NUL bytes
- rejects empty, `.`, and `..` path segments
- returns the normalized relative path as a kernel-owned buffer

`kernel_relativize_vault_path(...)`:

- requires an opened kernel handle with a vault root
- rejects null output, null handle, null non-empty input, empty host paths, and
  embedded NUL bytes
- normalizes the host path and vault root before comparison
- accepts only paths inside the opened vault root
- returns `/`-separated relative paths using generic path format
- rejects relative outputs that are rooted, drive-qualified, or contain `.`,
  `..`, or empty segments
- rejects the vault root itself unless `allow_empty != 0`
- returns an empty kernel-owned buffer for the vault root when
  `allow_empty != 0`
- validates non-empty outputs with `kernel_normalize_vault_relative_path(...)`
  semantics before returning

`kernel_read_vault_file(...)`:

- requires an opened kernel handle with a vault root
- accepts only host paths inside the opened vault root
- rejects null output, null handle, null non-empty input, empty host paths,
  embedded NUL bytes, and the vault root itself
- resolves the accepted host path through the canonical vault-relative path
  before reading
- returns exact raw file bytes, including embedded NUL bytes
- maps missing files and directories to the kernel file-read error path

## Host Boundary

Tauri Rust may still:

- receive file-change vectors from the watcher/front-end command payload
- pass the vectors through the sealed kernel bridge
- pass raw ignored-root CSV through the sealed kernel bridge
- use returned relative paths to request kernel note catalog/read surfaces
- use returned relative paths to emit supported vault file-change events
- use the same kernel path filter to validate one-off indexed Markdown note
  reads
- use the same kernel path filter before deleting kernel `ai_embedding_cache`
  rows
  for removed Markdown notes
- ignore directory events as a platform watcher concern before calling the
  kernel path filter
- pass notify watcher absolute event paths through
  `kernel_relativize_vault_path(...)` before supported-path filtering and IPC
  fanout
- use `kernel_normalize_vault_relative_path(...)` before one-off note read,
  write, and chemistry reference commands that accept a relative vault path
- pass absolute host paths from by-path note and entry commands through
  `kernel_relativize_vault_path(...)` before calling kernel read/write/delete,
  rename, move, or create-folder surfaces
- pass absolute host paths for media binary previews through
  `kernel_read_vault_file(...)`

Tauri Rust must not reimplement supported-extension filtering, changed-entry
Markdown filtering, hidden segment filtering, ignored-root filtering,
ignored-root normalization, path deduplication, or one-off vault relative path
validation/normalization. It must also not reimplement vault-root
relativization, `strip_prefix` checks, separator normalization, parent-segment
checks, root-folder allowance rules for by-path operations, or vault-root checks
before media binary reads.
Watcher event classification must also not use Rust `strip_prefix` to decide
vault membership or produce relative event paths.
