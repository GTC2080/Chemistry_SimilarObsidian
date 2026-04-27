<!-- Reason: This file freezes the kernel-owned file tree surface so Tauri Rust stops rebuilding vault hierarchy from note rows. -->

# File Tree Contract

## Scope

Current surface:

- `kernel_get_file_tree_default_limit(out_limit)`
- `kernel_query_file_tree(handle, limit, out_tree)`
- `kernel_query_file_tree_filtered(handle, limit, ignored_roots_csv, out_tree)`
- `kernel_free_file_tree(out_tree)`

The tree is derived from the same live note catalog used by `kernel_query_notes(...)`.

## Ownership

- the kernel owns returned tree nodes, note payload strings, and child arrays
- the kernel owns the default host-facing file-tree source catalog limit
  returned by `kernel_get_file_tree_default_limit(...)`
- callers release the whole tree with `kernel_free_file_tree(...)`
- `kernel_free_file_tree(...)` is idempotent and leaves `nodes == nullptr` and `count == 0`
- invalid handle, null output, or zero limit returns `KERNEL_ERROR_INVALID_ARGUMENT`

## Frozen Semantics

- folders sort before files at every level
- siblings with the same folder/file kind sort by `name`
- folder `file_count` is the recursive count of note leaves below it
- file leaves have `file_count == 1`
- file leaves carry a note payload with relative path, file stem, extension, and mtime
- path separators are normalized to `/`
- hosts that need the default file-tree source catalog limit must read it from
  `kernel_get_file_tree_default_limit(...)`, not from a Rust constant
- the limit applies to the source note catalog before tree construction
- filtered queries parse comma-separated ignored root names by trimming
  whitespace and leading/trailing slashes
- filtered queries remove only notes whose first relative-path segment exactly
  matches an ignored root
- root files with similar names remain visible unless the whole first segment
  matches an ignored root

## Host Boundary

Tauri Rust may still:

- attach host-specific absolute paths for existing frontend DTOs
- register the Tauri command and serialize the response

Tauri Rust must not rebuild folder hierarchy, sorting, recursive counts, or
ignored-root filtering from raw note rows. It must also not retain a duplicate
default file-tree query limit.
