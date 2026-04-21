<!-- Reason: This file freezes the Track 3 PDF regression obligations so metadata, anchor, and reference behavior grow by explicit repository rules instead of host inference. -->

# PDF Regression Matrix

Last updated: `2026-04-22`

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
- note -> PDF refs order is stable
- PDF -> note referrers order is stable
- missing PDF attachments surface the frozen missing reference state
- stale anchors surface the frozen stale reference state
- unresolved anchors surface the frozen unresolved reference state
- PDF references do not enter the existing backlinks public surface
- PDF references do not create new search-hit kinds in the existing search surface

## Batch 4: PDF Diagnostics / Rebuild / Gates

The repository must retain regression coverage for:

- Batch 1 diagnostics export exposes PDF contract revision, extract mode, and lookup-key mode
- Batch 2 diagnostics export extends PDF diagnostics with anchor mode
- diagnostics export keys per-document PDF summaries by normalized live attachment `rel_path`
- rebuild realigns PDF-derived state to the same truth as a clean startup on the same vault snapshot
- recovery realigns PDF-derived state to the same truth as rebuild on the same vault snapshot
- watcher overflow / full rescan realigns PDF-derived state to the same truth as rebuild on the same vault snapshot
- benchmark gates cover metadata extraction, anchor rebuild, note -> PDF refs query, and PDF -> note referrers query
