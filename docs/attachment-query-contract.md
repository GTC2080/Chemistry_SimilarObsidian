<!-- Reason: This file freezes the Track 2 Batch 1 host-facing attachment query contract so hosts can depend on a single live-catalog surface instead of reconstructing attachment state from SQLite internals. -->

# Attachment Query Contract

Last updated: `2026-04-21`

## Scope

This document freezes the formal host-facing attachment read surface for:

- `kernel_query_attachments(...)`
- `kernel_get_attachment(...)`
- `kernel_query_note_attachment_refs(...)`
- `kernel_query_attachment_referrers(...)`
- `kernel_free_attachment_record(...)`
- `kernel_free_attachment_list(...)`
- `kernel_free_attachment_referrers(...)`

This is the official live-catalog contract.
It supersedes the earlier minimal-only attachment surface for host integration.

## Live Catalog Boundary

The public attachment surface is defined against the live attachment catalog.

Live catalog rules:

- an attachment is in the live catalog when at least one live note currently references its normalized `rel_path`
- a live attachment remains in the catalog when its disk presence is `missing`
- a disk file that exists but is not referenced by any live note is not part of the live catalog
- an orphaned metadata row in `attachments` with zero live refs is not part of the live catalog

Observable consequences:

- `kernel_query_attachments(...)` only returns live catalog entries
- `kernel_get_attachment(...)` only succeeds for live catalog entries
- `kernel_query_attachment_referrers(...)` only succeeds for live catalog entries
- unreferenced files and orphaned metadata rows return `KERNEL_ERROR_NOT_FOUND`

## Public Record Shape

`kernel_attachment_record` freezes these host-facing fields:

Field-level metadata semantics are frozen in:

- [attachment-metadata-contract.md](/E:/测试/Chemistry_Obsidian/docs/attachment-metadata-contract.md)

- `rel_path`
  - normalized vault-relative attachment path
  - unique identifier for one live catalog entry
- `basename`
  - final path segment from `rel_path`
  - display helper only
- `extension`
  - lowercase filename extension including the leading `.`
  - empty string when the path has no extension
- `file_size`
  - last reconciled file size in bytes
- `mtime_ns`
  - last reconciled modification timestamp in nanoseconds
- `ref_count`
  - number of live notes that currently reference the attachment
- `kind`
  - coarse attachment kind derived from the frozen extension mapping
- `flags`
  - currently `KERNEL_ATTACHMENT_FLAG_NONE`
  - hosts must treat unknown future bits as additive
- `presence`
  - `KERNEL_ATTACHMENT_PRESENCE_PRESENT` or `KERNEL_ATTACHMENT_PRESENCE_MISSING`

## Attachment Kind Mapping

The current public kind mapping revision is:

- `attachment_kind_mapping_revision = track2_batch1_extension_mapping_v1`

Current coarse kind values:

- `KERNEL_ATTACHMENT_KIND_UNKNOWN`
- `KERNEL_ATTACHMENT_KIND_GENERIC_FILE`
- `KERNEL_ATTACHMENT_KIND_IMAGE_LIKE`
- `KERNEL_ATTACHMENT_KIND_PDF_LIKE`
- `KERNEL_ATTACHMENT_KIND_CHEM_LIKE`

Current extension mapping:

- `image_like`
  - `.png`, `.jpg`, `.jpeg`, `.gif`, `.bmp`, `.webp`, `.svg`, `.tif`, `.tiff`
- `pdf_like`
  - `.pdf`
- `chem_like`
  - `.mol`, `.mol2`, `.sdf`, `.sd`, `.pdb`, `.cif`, `.xyz`, `.cdx`, `.cdxml`, `.rxn`
- `generic_file`
  - any other non-empty extension
- `unknown`
  - no extension

## Query Surface

### `kernel_query_attachments(...)`

Returns the live attachment catalog.

Input rules:

- `handle` must be non-null
- `out_attachments` must be non-null
- `limit` must be greater than `0`
- `limit = (size_t)-1` means unbounded list within the current snapshot

Result rules:

- success with no live attachments returns `KERNEL_OK` and `count == 0`
- order is `rel_path ASC`
- one live attachment path appears at most once

### `kernel_get_attachment(...)`

Returns one live attachment record by normalized `rel_path`.

Input rules:

- `handle` must be non-null
- `out_attachment` must be non-null
- `attachment_rel_path` must be a valid non-empty relative path
- absolute paths, rooted paths, and paths containing `..` are invalid

Lookup rules:

- success requires the path to be in the live catalog
- a live `missing` attachment still succeeds
- an unreferenced disk file returns `KERNEL_ERROR_NOT_FOUND`
- an orphaned metadata row returns `KERNEL_ERROR_NOT_FOUND`

### `kernel_query_note_attachment_refs(...)`

Returns the live attachment records currently referenced by one live note.

Input rules:

- `handle` must be non-null
- `out_attachments` must be non-null
- `note_rel_path` must be a valid non-empty relative path
- `limit` must be greater than `0`

Result rules:

- a live note with no attachment refs returns `KERNEL_OK` and `count == 0`
- a missing or deleted note returns `KERNEL_ERROR_NOT_FOUND`
- order is the persisted note attachment ref order

### `kernel_query_attachment_referrers(...)`

Returns the live notes that currently reference one live attachment.

Input rules:

- `handle` must be non-null
- `out_referrers` must be non-null
- `attachment_rel_path` must be a valid non-empty relative path
- `limit` must be greater than `0`

Result rules:

- order is `note_rel_path ASC`
- a path outside the live catalog returns `KERNEL_ERROR_NOT_FOUND`

Each `kernel_attachment_referrer` exposes:

- `note_rel_path`
- `note_title`

## Presence Semantics

`presence` is frozen to:

- `present`
  - the latest reconciled disk truth observed a regular file at the attachment path
- `missing`
  - the latest reconciled disk truth observed the attachment path as absent

Missing semantics:

- missing attachments remain queryable while they stay in the live catalog
- when a missing attachment was never previously observed on disk, `file_size` and `mtime_ns` may be `0`
- when a previously present attachment becomes missing, `file_size` and `mtime_ns` preserve the last reconciled values

## Consistency With Search

The attachment public surface and the existing attachment path search surface must agree.

Frozen consistency rules:

- if an attachment is visible through `kernel_query_attachments(...)` or `kernel_get_attachment(...)`, it is eligible to appear in attachment path search results
- if an attachment is excluded from the live catalog, it must not appear in attachment path search results
- orphaned attachment metadata rows are excluded from both surfaces
- unreferenced disk files are excluded from both surfaces

## Memory Ownership

`kernel_attachment_record`, `kernel_attachment_list`, and `kernel_attachment_referrers` are kernel-owned result objects.

Rules:

- `kernel_free_attachment_record(...)` is idempotent
- `kernel_free_attachment_list(...)` is idempotent
- `kernel_free_attachment_referrers(...)` is idempotent
- callers may reuse the same output struct across calls
- each query resets any previously owned output before writing new results
- on failure, output objects are returned empty

## Error Surface

Expected public error classes:

- `KERNEL_OK`
- `KERNEL_ERROR_INVALID_ARGUMENT`
- `KERNEL_ERROR_NOT_FOUND`
- `KERNEL_ERROR_IO`
- `KERNEL_ERROR_INTERNAL`

Rules:

- invalid path or null output is `KERNEL_ERROR_INVALID_ARGUMENT`
- a live note with zero attachment refs is an empty successful result
- a missing note is `KERNEL_ERROR_NOT_FOUND`
- a non-live attachment path is `KERNEL_ERROR_NOT_FOUND`

## Legacy Minimal Attachment ABI

These earlier entry points remain supported for Phase 1 compatibility:

- `kernel_list_note_attachments(...)`
- `kernel_get_attachment_metadata(...)`
- `kernel_free_attachment_refs(...)`

Their status is:

- deprecated but supported
- no new Track 2 capabilities will be added to them
- new host integrations must use the formal live-catalog surface above
