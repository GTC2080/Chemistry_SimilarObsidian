<!-- Reason: This file records regression obligations for the kernel-owned file tree surface. -->

# File Tree Regression Matrix

## Kernel File Tree

Required coverage:

- `kernel_query_file_tree(...)` builds a nested tree from live note catalog rows
- folders sort before files
- sibling folders/files sort by `name`
- nested folder `file_count` includes all descendant note leaves
- file leaves preserve relative path, stem name, extension, and mtime payload
- `kernel_free_file_tree(...)` resets output and is safe on repeat calls
- `kernel_query_file_tree_filtered(...)` removes notes below exactly matching
  ignored top-level roots
- filtered file tree queries trim whitespace and leading/trailing slashes from
  ignored root names
- filtered file tree queries keep root files with names similar to ignored
  folder names
- zero limit returns `KERNEL_ERROR_INVALID_ARGUMENT`
- null handle returns `KERNEL_ERROR_INVALID_ARGUMENT`
- null output returns `KERNEL_ERROR_INVALID_ARGUMENT`

## Tauri Bridge

Required coverage:

- `build_file_tree` consumes the kernel file tree instead of rebuilding from note infos
- ignored top-level folders are passed through the Tauri native bridge into the
  kernel filtered file tree surface
