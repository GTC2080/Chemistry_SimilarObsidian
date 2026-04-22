<!-- Reason: This file freezes the Phase 1 host-facing attachment read contract so hosts do not have to infer attachment semantics from SQLite internals or watcher side effects. -->

# Attachment Public Surface

This document now describes the legacy minimal attachment ABI only.
The formal Track 2 live-catalog contract lives at
[attachment-query-contract.md](/E:/测试/Chemistry_Obsidian/kernel/docs/contracts/attachment-query-contract.md).

Last updated: `2026-04-20`

## Scope

This document freezes the legacy Phase 1 host-facing behavior for:

- `kernel_list_note_attachments(...)`
- `kernel_get_attachment_metadata(...)`
- `kernel_free_attachment_refs(...)`
- diagnostics fields `attachment_count` and `missing_attachment_count`

It remains host-visible for compatibility.
It is no longer the preferred attachment integration surface.

## Shared ABI Contract

`kernel_list_note_attachments(...)` returns `kernel_attachment_refs`, which owns an array of `kernel_attachment_ref`.
Each ref exposes:

- `rel_path`: UTF-8 relative attachment path as normalized and stored by the kernel

Memory rules:

- `out_refs` must be non-null
- returned strings and the result array are kernel-owned until `kernel_free_attachment_refs(...)`
- `kernel_free_attachment_refs(...)` is idempotent and leaves the struct empty
- attachment list APIs may reuse the same `kernel_attachment_refs` struct across calls
- if `out_refs` already contains kernel-owned refs from a previous successful call, the next call releases them before writing new output
- if a list call fails and `out_refs` is non-null, it returns an empty result object:
  - `refs == nullptr`
  - `count == 0`

`kernel_get_attachment_metadata(...)` fills a by-value `kernel_attachment_metadata`:

- `file_size`: last reconciled file size in bytes
- `mtime_ns`: last reconciled modification timestamp in nanoseconds
- `is_missing`: `1` when the attachment path is currently missing from disk, otherwise `0`

Failure behavior:

- `out_metadata` must be non-null
- on failure, metadata output is reset to zero values before returning

## Note Attachment List Contract

`kernel_list_note_attachments(...)` returns the attachment refs currently derived from one active note.

Input rules:

- `handle` must be non-null
- `note_rel_path` must be a non-empty relative path
- absolute paths are invalid
- rooted paths are invalid
- any path containing `..` is invalid

Path semantics:

- input note paths are normalized with the same lexical relative-path normalization used elsewhere in the kernel
- hosts may pass either forward slashes or Windows separators in a valid relative path
- lookup is by normalized note relative path

Result semantics:

- result order is parser order, not sorted order
- duplicate refs are preserved if the note text repeats the same attachment path
- an existing note with no attachment refs returns `KERNEL_OK` with `count == 0`
- a missing or deleted note returns `KERNEL_ERROR_NOT_FOUND`

## Attachment Metadata Contract

`kernel_get_attachment_metadata(...)` returns the current reconciled metadata for one normalized attachment path.

Input rules:

- `handle` must be non-null
- `attachment_rel_path` must be a non-empty relative path
- absolute paths are invalid
- rooted paths are invalid
- any path containing `..` is invalid

Metadata semantics:

- metadata is keyed by normalized relative attachment path
- `is_missing == 0` means the latest indexed disk truth observed that path as a regular file
- `is_missing == 1` means the latest indexed disk truth observed that path as absent
- when a missing attachment was never seen on disk, `file_size` and `mtime_ns` may both be `0`
- when a previously present attachment becomes missing, `file_size` and `mtime_ns` remain the last reconciled values if the kernel already knew them

Lookup semantics:

- a path with no indexed attachment metadata returns `KERNEL_ERROR_NOT_FOUND`
- metadata lookup is read-only and does not mutate watcher or rebuild state

## Consistency Contract

These attachment read surfaces observe derived SQLite state, but their observable semantics are frozen around disk truth:

- after a successful `kernel_write_note(...)`, stale note attachment refs for that note are replaced
- after note rewrite, old refs that no longer appear in the note stop appearing in `kernel_list_note_attachments(...)`
- after startup recovery finishes, recovered disk truth replaces stale attachment refs and attachment metadata
- after rebuild finishes successfully, disk truth replaces stale attachment metadata and note attachment refs
- after watcher-driven attachment rename reconciliation, note refs can point at the new path and metadata for the old/new paths reflects the reconciled missing/present state

Hosts should treat attachment results as stable only after the runtime has reached a healthy queryable state such as `READY`.

## Diagnostics Contract

Diagnostics now export:

- `attachment_count`
- `missing_attachment_count`

These counts are frozen to the host-facing attachment surface:

- `attachment_count` is the count of distinct normalized attachment paths currently referenced by active notes
- `missing_attachment_count` is the subset of those distinct referenced paths whose attachment metadata currently reports `is_missing=1`

This intentionally excludes unrelated non-markdown scratch paths such as transient temp files that may be observed internally by watcher or save flows.

## Error Surface

These attachment APIs use `kernel_status.code` only.

Expected public error classes:

- `KERNEL_OK`
- `KERNEL_ERROR_INVALID_ARGUMENT`
- `KERNEL_ERROR_NOT_FOUND`
- `KERNEL_ERROR_IO`

`KERNEL_ERROR_INVALID_ARGUMENT` covers the invalid-input cases listed above.
Attachment misses are `KERNEL_ERROR_NOT_FOUND`, not empty success.
