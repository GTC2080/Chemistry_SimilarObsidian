<!-- Reason: This file freezes the Track 3 PDF regression obligations so metadata, anchor, and reference behavior grow by explicit repository rules instead of host inference. -->

# PDF Regression Matrix

Last updated: `2026-05-02`

## Frozen Cross-Batch Invariants

The repository must retain regression coverage for:

- `pdf_metadata_revision = f(attachment_content_revision, pdf_extract_mode_revision)`
- `pdf_anchor_basis_revision = f(attachment_content_revision, pdf_anchor_mode_revision, anchor_relevant_text_basis)`
- the canonical anchor shape `rel_path + pdf_anchor_basis_revision + page + excerpt_fingerprint`
- all PDF-facing public lookups using normalized live attachment `rel_path` as the only document key
- PDF remaining an attachment subtype rather than an independent truth source
- PDF references remaining outside the existing backlinks public surface
- PDF references not adding new search-hit kinds to the existing search surface

## Batch 1: PDF Metadata Contract v1

The repository must retain regression coverage for:

- `kernel_get_pdf_metadata(...)` succeeds for live `pdf_like` attachments
- non-PDF attachment lookup is rejected
- non-live attachment-path lookup is rejected
- `page_count` follows disk truth after watcher refresh
- `has_outline` follows disk truth after rebuild
- `doc_title` follows the frozen absent / unavailable downgrade rules
- `text_layer_state` follows the frozen extract-state rules
- `pdf_metadata_revision` changes when the PDF content revision changes
- `pdf_metadata_revision` changes when `pdf_extract_mode_revision` changes
- `pdf_metadata_revision` does not change only because recovery, rebuild, or watcher refresh reached the same disk truth through a different path

## Batch 2: PDF Source Anchor Model v1

The repository must retain regression coverage for:

- anchor serialization roundtrips without drift
- identical disk truth rebuilds the same anchor
- `pdf_anchor_mode` is exported in diagnostics once Batch 2 lands
- `pdf_anchor_basis_revision` changes when anchor-relevant text basis changes
- `pdf_anchor_basis_revision` changes when `pdf_anchor_mode_revision` changes
- `pdf_anchor_basis_revision` may ignore unrelated document-level metadata bytes as long as the anchor-relevant page-content basis is unchanged
- `pdf_anchor_basis_revision` does not change only because unrelated metadata such as `doc_title` or `has_outline` changed
- page-level anchor validation distinguishes resolved, stale, unverifiable, and unavailable states
- rebuild and recovery reconstitute anchors to the same canonical serialized form for the same disk truth

## Batch 3: Note ↔ PDF Reference Public Surface v1

The repository must retain regression coverage for:

- note -> PDF refs return the frozen `pdf_rel_path`
- PDF -> note referrers return the same normalized live attachment `rel_path`
- markdown `#anchor=` links are the only formal note-side source-ref carrier
- note -> PDF refs order is stable
- PDF -> note referrers order is stable
- plain PDF attachment links do not appear in the formal PDF reference surfaces
- missing PDF attachments surface the frozen missing reference state
- stale anchors surface the frozen stale reference state
- unresolved anchors surface the frozen unresolved reference state
- note -> PDF refs return stable page numbers when the serialized anchor carries a valid page
- stale reference rows preserve their stored excerpt snapshot
- live PDFs with zero formal source refs return an empty PDF -> note referrers list
- non-live PDF document keys return `NOT_FOUND` for PDF -> note referrers
- PDF references do not enter the existing backlinks public surface
- PDF references do not create new search-hit kinds in the existing search surface

## Batch 4: PDF Diagnostics / Rebuild / Gates

The repository must retain regression coverage for:

- Batch 1 diagnostics export exposes PDF contract revision, extract mode, and lookup-key mode
- Batch 2 diagnostics export extends PDF diagnostics with anchor mode
- Batch 4 diagnostics export exposes anchor counts, source-ref counts, anomaly summaries, and last PDF recount fields
- diagnostics export keys per-document PDF summaries by normalized live attachment `rel_path`
- rebuild realigns PDF-derived state to the same truth as a clean startup on the same vault snapshot
- recovery realigns PDF-derived state to the same truth as rebuild on the same vault snapshot
- watcher overflow / full rescan realigns PDF-derived state to the same truth as rebuild on the same vault snapshot
- benchmark gates cover metadata extraction, anchor rebuild, note -> PDF refs query, and PDF -> note referrers query

## Stateless PDF Ink Smoothing

The repository must retain regression coverage for:

- `kernel_get_pdf_ink_default_tolerance(...)` returning the kernel-owned default
  tolerance and rejecting null output
- `kernel_smooth_ink_strokes(...)` interpolating a curved three-point stroke
- first and last points remaining anchored
- stroke width being preserved
- two-point strokes remaining two points
- point pressure being preserved
- null output returning invalid argument
- nonzero stroke count with null strokes returning invalid argument
- nonzero point count with null points returning invalid argument
- `kernel_free_ink_smoothing_result(...)` resetting output pointers and being repeat-safe
- Tauri sealed bridge serializes ink smoothing kernel results to JSON without
  retaining Rust-owned ink C ABI structs or result-copy loops
- Tauri `smooth_ink_strokes` reads the default tolerance through the sealed
  kernel bridge instead of a duplicate Rust constant

## Stateless PDF Annotation Hashing

The repository must retain regression coverage for:

- `kernel_compute_pdf_annotation_storage_key(...)` returning the first 16 hex
  characters of the PDF path SHA-256
- `kernel_compute_pdf_annotation_storage_key(...)` rejecting null or empty path
  and null output
- `kernel_compute_pdf_lightweight_hash(...)` hashing
  `first_1kb || last_1kb || file_size_le`
- `kernel_compute_pdf_lightweight_hash(...)` rejecting null non-empty head,
  null non-empty tail, and null output
- Tauri PDF annotation persistence reading both rules through the sealed kernel
  bridge instead of using Rust-side SHA-256
- Tauri PDF annotation load/save commands passing absolute viewer file paths
  through `kernel_relativize_vault_path(...)` before choosing the JSON storage
  key
- Tauri PDF annotation JSON storing the same vault-relative `pdfPath` used for
  the kernel storage key, while retaining the absolute host path only for file
  I/O and lightweight content hashing
