<!-- Reason: This file freezes the Track 2 Batch 2 attachment metadata contract so hosts can depend on stable field meanings across watcher refresh, rebuild, and recovery without inspecting SQLite internals. -->

# Attachment Metadata Contract

Last updated: `2026-04-21`

## Scope

This document freezes the host-facing metadata semantics carried by:

- `kernel_attachment_record`
- `kernel_attachment_list`
- `kernel_query_attachments(...)`
- `kernel_get_attachment(...)`
- `kernel_query_note_attachment_refs(...)`

The attachment metadata contract revision is:

- `attachment_metadata_contract_revision = track2_batch2_metadata_contract_v1`

## Frozen Fields

The following fields are stable host-facing metadata:

- `rel_path`
  - normalized vault-relative attachment path
  - unique within the live attachment catalog
- `basename`
  - final path segment from `rel_path`
- `extension`
  - lowercase extension including the leading `.`
  - empty string when the attachment path has no extension
- `file_size`
  - last reconciled disk file size in bytes
- `mtime_ns`
  - last reconciled disk modification time in nanoseconds
- `presence`
  - `KERNEL_ATTACHMENT_PRESENCE_PRESENT`
  - `KERNEL_ATTACHMENT_PRESENCE_MISSING`
- `ref_count`
  - count of live note refs to the attachment
- `kind`
  - coarse-grained attachment kind from the frozen extension mapping
- `flags`
  - currently `KERNEL_ATTACHMENT_FLAG_NONE`
  - unknown future bits are additive only

## Presence Semantics

- `present`
  - the latest reconciled disk truth observed a regular file at `rel_path`
- `missing`
  - the latest reconciled disk truth observed no regular file at `rel_path`

Frozen rules:

- a live attachment may remain queryable while `presence == missing`
- if an attachment was never observed present on disk, `file_size` and `mtime_ns` may be `0`
- if an attachment transitions from `present` to `missing`, `file_size` and `mtime_ns` preserve the last reconciled present values

## Kind Mapping

The current mapping revision is:

- `attachment_kind_mapping_revision = track2_batch1_extension_mapping_v1`

Frozen coarse kinds:

- `KERNEL_ATTACHMENT_KIND_UNKNOWN`
- `KERNEL_ATTACHMENT_KIND_GENERIC_FILE`
- `KERNEL_ATTACHMENT_KIND_IMAGE_LIKE`
- `KERNEL_ATTACHMENT_KIND_PDF_LIKE`
- `KERNEL_ATTACHMENT_KIND_CHEM_LIKE`

Frozen mapping:

- `unknown`
  - empty extension
- `image_like`
  - `.png`, `.jpg`, `.jpeg`, `.gif`, `.bmp`, `.webp`, `.svg`, `.tif`, `.tiff`
- `pdf_like`
  - `.pdf`
- `chem_like`
  - `.mol`, `.mol2`, `.sdf`, `.sd`, `.pdb`, `.cif`, `.xyz`, `.cdx`, `.cdxml`, `.rxn`
- `generic_file`
  - any other non-empty extension

## Lifecycle Stability

Metadata semantics are frozen across the current kernel flows:

- watcher refresh
  - create and modify update `file_size`, `mtime_ns`, `presence`, and `kind`
  - delete flips `presence` to `missing` and preserves last reconciled `file_size` and `mtime_ns`
- rebuild
  - rebuild recomputes metadata from disk truth and live note refs
- startup recovery / reopen catch-up
  - recovery and catch-up realign metadata to disk truth while preserving the live-catalog boundary

## Search Consistency

Attachment metadata and the search public surface must agree on visibility:

- live catalog attachments may appear in attachment path search
- orphaned metadata rows must not appear in either surface
- unreferenced disk files must not appear in either surface

## Diagnostics

Support-bundle diagnostics must export:

- `attachment_public_surface_revision`
- `attachment_metadata_contract_revision`
- `attachment_kind_mapping_revision`
- `attachment_live_count`
- `missing_attachment_count`
