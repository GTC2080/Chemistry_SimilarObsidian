<!-- Reason: This file freezes the Track 3 execution order and pre-implementation rules so PDF substrate work lands on the stable attachment/search kernel baseline without drifting into reader-shell scope. -->

# PDF Track 3 Backlog

Last updated: `2026-04-22`

## Scope

Track 3 opens the PDF kernel capability layer on top of the formal attachment surface.

Track 3 remains inside the kernel boundary:

- PDF is an attachment subtype, not an independent truth source
- PDF-derived state must rebuild, recover, and rescan from the same disk truth as attachments
- Track 3 does not add reader UI, OCR, AI summarization, chemistry-specific deep parsing, plugin hooks, sync, or multi-vault behavior

## Frozen Cross-Batch Rules

These three rules are fixed before implementation.

### Rule 1: Metadata Revision

- `pdf_metadata_revision = f(attachment_content_revision, pdf_extract_mode_revision)`
- `pdf_metadata_revision` must change when either:
  - the underlying PDF file content revision changes
  - the frozen metadata extractor mode revision changes
- `pdf_metadata_revision` must not depend on watcher timing, rebuild path, recovery path, or host cache state

### Rule 2: Anchor Basis Revision

- `pdf_anchor_basis_revision = f(attachment_content_revision, pdf_anchor_mode_revision, anchor_relevant_text_basis)`
- the canonical Batch 2 anchor model is:
  - `rel_path + pdf_anchor_basis_revision + page + excerpt_fingerprint`
- anchor validity must not depend on unrelated metadata fields such as `doc_title` or `has_outline`
- changes that do not affect anchor-relevant text basis must not force anchors stale

### Rule 3: PDF Document Key

- all PDF-facing public lookup surfaces use `normalized live attachment rel_path` as the only document key
- this rule applies to:
  - PDF metadata lookup
  - PDF referrers lookup
  - note -> PDF refs `pdf_rel_path`
  - diagnostics summary keys
  - support-bundle export keys
- row ids, internal metadata keys, and UI object ids must not become public PDF identity keys

## Batch 1: PDF Metadata Contract v1

Goal:

- promote `pdf_like` from a coarse attachment kind to a formal PDF metadata surface

Required work:

- add a formal PDF metadata ABI keyed by normalized live attachment `rel_path`
- freeze these metadata fields:
  - `page_count`
  - `has_outline`
  - `doc_title`
  - `text_layer_state`
  - `pdf_metadata_revision`
- define stable `ready / partial / unavailable / invalid_pdf` metadata-state semantics
- define stable rebuild / recovery / watcher-refresh behavior for metadata recomputation
- document that PDF metadata remains derived from the attachment truth, not a second catalog

Required documents:

- `pdf-query-contract.md`

Acceptance:

- hosts can read stable PDF metadata without inspecting SQLite or re-parsing files themselves

## Batch 2: PDF Source Anchor Model v1

Goal:

- freeze a minimal page-level source anchor that is serializable, rebuildable, and diagnosable

Required work:

- add page-level anchor records and canonical serialized form
- freeze minimal excerpt preview and `excerpt_fingerprint` semantics
- bind anchors to:
  - normalized live attachment `rel_path`
  - `pdf_anchor_basis_revision`
  - page
  - excerpt fingerprint
- define stale, unverifiable, and unavailable downgrade states
- define rebuild / recovery / watcher-refresh anchor rebuild rules

Required documents:

- `pdf-query-contract.md`

Acceptance:

- anchors can be reconstructed from disk truth and validated without any viewer-side selection model

## Batch 3: Note ↔ PDF Reference Public Surface v1

Goal:

- expose stable note -> PDF anchor refs and PDF -> note referrers through formal public ABI

Required work:

- add note -> PDF source reference queries
- add PDF -> note referrer queries
- freeze result ordering, `ref_state`, excerpt preview, and missing/stale/unresolved semantics
- keep PDF references outside the existing backlinks public surface
- keep PDF references outside the existing search-hit surface
- keep attachment `rel_path` as the only public PDF document key

Required documents:

- `pdf-query-contract.md`
- `pdf-regression-matrix.md`

Acceptance:

- hosts can read note ↔ PDF relationships directly without reconstructing joins from attachment rows and note text

## Batch 4: PDF Diagnostics / Rebuild / Gates

Goal:

- bring PDF-derived state up to the same supportability level as search and attachments

Required work:

- add PDF metadata, anchor, and reference summaries to the support bundle
- freeze rebuild / recovery / watcher-overflow recount rules for PDF-derived state
- add PDF benchmark coverage
- add PDF regression-matrix coverage
- wire PDF checks into the same repository-local gating discipline used for the existing stable lines

Required documents:

- `pdf-regression-matrix.md`

Acceptance:

- PDF-derived state can be rebuilt, recovered, rescanned, and diagnosed without host-side inference
