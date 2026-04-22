<!-- Reason: This file freezes the cross-batch Track 3 PDF query invariants so PDF metadata, anchors, and note references land on one stable document identity without reopening reader-shell scope. -->

# PDF Query Contract

Last updated: `2026-04-22`

## Scope

This document freezes the host-facing invariants that all Track 3 PDF surfaces must obey.

It applies to:

- the Batch 1 PDF metadata lookup surface
- future note -> PDF source-reference surfaces
- future PDF -> note referrer surfaces
- PDF diagnostics export keys
- support-bundle PDF document identity

Batch 1 lands:

- `kernel_get_pdf_metadata(handle, attachment_rel_path, out_metadata)`

Batch 3 lands:

- `kernel_query_note_pdf_source_refs(handle, note_rel_path, limit, out_refs)`
- `kernel_query_pdf_referrers(handle, attachment_rel_path, limit, out_referrers)`

## Attachment Boundary

PDF remains an attachment subtype.

Frozen boundary rules:

- PDF does not create an independent document catalog
- PDF-derived state is derived from the live attachment catalog and the same disk truth used by attachment rebuild, recovery, and watcher refresh
- a non-live attachment path is not a valid PDF document key
- an unreferenced PDF file on disk is outside the PDF public surface

## Frozen Document Identity Rule

All PDF-facing public lookup surfaces use:

- `normalized live attachment rel_path`

as the only public document key.

This rule is frozen for:

- PDF metadata lookup
- PDF referrers lookup
- note -> PDF refs `pdf_rel_path`
- diagnostics summary keys
- support-bundle export keys

Public PDF identity must not be:

- a storage row id
- an internal metadata row key
- a UI document object id
- a viewer session id

## Frozen Metadata Revision Rule

The metadata revision is:

- `pdf_metadata_revision = f(attachment_content_revision, pdf_extract_mode_revision)`

Frozen semantics:

- `attachment_content_revision`
  - the attachment-content truth revision for the current PDF file
- `pdf_extract_mode_revision`
  - the frozen revision of the metadata extractor mode used to derive PDF metadata
- `pdf_metadata_revision` must change when either input changes
- `pdf_metadata_revision` must not change only because rebuild, recovery, reopen catch-up, or watcher refresh reached the same disk truth through a different path

Metadata fields that are unrelated to content truth must not define document identity.

## Frozen Anchor Basis Revision Rule

The anchor basis revision is:

- `pdf_anchor_basis_revision = f(attachment_content_revision, pdf_anchor_mode_revision, anchor_relevant_text_basis)`

Frozen semantics:

- `pdf_anchor_basis_revision` is narrower than `pdf_metadata_revision`
- anchor validity depends only on inputs needed to rebuild and verify anchors
- the `attachment_content_revision` term in the anchor-basis function is the
  content-truth revision of the anchor-relevant PDF page slice used to rebuild
  the page anchor, not unrelated document-level metadata bytes
- metadata-only changes that do not affect anchor-relevant text basis must not stale an otherwise valid anchor
- changing `doc_title` or `has_outline` alone must not force anchor invalidation

## Frozen Anchor Shape Rule

The canonical Track 3 Batch 2 anchor shape is:

- `rel_path + pdf_anchor_basis_revision + page + excerpt_fingerprint`

Frozen semantics:

- `rel_path`
  - normalized live attachment `rel_path`
- `pdf_anchor_basis_revision`
  - the anchor rebuild basis revision defined above
- `page`
  - stable page identifier in the canonical PDF anchor surface
- `excerpt_fingerprint`
  - deterministic fingerprint of the normalized excerpt basis used to validate the source anchor

Track 3 does not add:

- screen-space coordinates
- viewport coordinates
- selection geometry
- highlight styling
- viewer interaction state

## Surface Consistency Rules

Track 3 must remain consistent with the existing stable surfaces.

Frozen consistency rules:

- `kernel_get_pdf_metadata(...)` only succeeds for normalized live attachment `rel_path`
- `kernel_get_pdf_metadata(...)` returns `NOT_FOUND` for non-PDF live attachments
- `kernel_get_pdf_metadata(...)` returns `NOT_FOUND` for non-live attachment paths
- PDF reference queries must reuse the same normalized live attachment `rel_path`
- PDF references do not enter the existing backlinks public surface
- PDF references do not add new search-hit kinds or new attachment content-search semantics
- attachment path search remains attachment-path-only even when the attachment is `pdf_like`

## Batch 3 Note-Reference Carrier

Track 3 Batch 3 freezes one note-side carrier shape:

- markdown local links with `#anchor=` on a PDF attachment target

Frozen note-side source-reference shape:

- `[Label](assets/paper.pdf#anchor=<canonical_pdf_anchor>)`

Frozen semantics:

- the PDF document key is the normalized live attachment `rel_path` before `#anchor=`
- the source anchor payload is the raw canonical serialized anchor after `#anchor=`
- plain attachment links such as `[Paper](assets/paper.pdf)` stay attachment refs only
- embed syntax does not become a separate PDF source-reference contract
- note-side PDF source refs must not require the host to parse note text or join SQLite tables itself

## Batch 3 Public Result State

Track 3 Batch 3 freezes these public states for note ↔ PDF references:

- `resolved`
- `missing`
- `stale`
- `unresolved`

Frozen semantics:

- `resolved`
  - the live PDF attachment is present and the canonical serialized anchor still validates against the current derived PDF anchor row
- `missing`
  - the live attachment path remains in the live catalog through note references but the file is currently missing
- `stale`
  - the live PDF attachment is present but the canonical serialized anchor no longer matches the current derived anchor for that page
- `unresolved`
  - the anchor cannot be parsed or cannot be validated to a resolved/stale state on the current live PDF truth

## Batch 3 Ordering And Lookup Rules

Track 3 Batch 3 freezes these result-ordering and lookup rules:

- note -> PDF refs are ordered by note source order
- PDF -> note referrers are ordered by `note_rel_path ASC`, then note-local source-ref order
- `kernel_query_note_pdf_source_refs(...)` returns `NOT_FOUND` for non-live notes
- `kernel_query_pdf_referrers(...)` returns `NOT_FOUND` for non-live PDF document keys
- `kernel_query_pdf_referrers(...)` returns an empty list for live PDF attachments that currently have no formal source refs
- plain attachment refs do not appear in note -> PDF refs or PDF -> note referrers
- the host does not reconstruct note ↔ PDF relationships from attachments, backlinks, or search results

## Lifecycle Consistency Rules

Track 3 must stay aligned with the existing lifecycle contracts.

Frozen rules:

- rebuild recomputes PDF-derived state from disk truth and live attachment truth
- recovery recomputes or repairs PDF-derived state to the same stable truth
- watcher refresh and watcher full rescan must converge to the same PDF-derived result as rebuild for the same disk snapshot
- identical disk truth must produce identical normalized `rel_path`, `pdf_metadata_revision`, and `pdf_anchor_basis_revision`

## Diagnostics Keys

Support-bundle and diagnostics exports must key PDF document summaries by:

- normalized live attachment `rel_path`

Batch 1 diagnostics contract exposes:

- `pdf_contract_revision`
- `pdf_extract_mode`
- `pdf_lookup_key_mode`

Batch 2 extends diagnostics with:

- `pdf_anchor_mode`

Batch 4 extends diagnostics with:

- `pdf_live_anchor_count`
- `pdf_source_ref_count`
- `pdf_source_ref_resolved_count`
- `pdf_source_ref_missing_count`
- `pdf_source_ref_stale_count`
- `pdf_source_ref_unresolved_count`
- `pdf_metadata_anomaly_summary`
- `pdf_reference_anomaly_summary`
- `last_pdf_recount_reason`
- `last_pdf_recount_at_ns`

Batch 2 anchor validation distinguishes:

- `resolved`
- `stale`
- `unverifiable`
- `unavailable`

## Remaining Track 3 Surface

Track 3 Batch 1-3 now uses this document as the frozen host-facing contract.

Later Track 3 work must extend these rules without reopening:

- document identity
- metadata revision composition
- anchor basis revision composition
- note-reference carrier shape
- attachment/search/backlinks consistency boundaries
