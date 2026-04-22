<!-- Reason: This file freezes the Track 4 regression obligations so multi-domain substrate behavior grows by explicit repository rules instead of per-discipline special cases. -->

# Domain Regression Matrix

Last updated: `2026-04-22`

## Frozen Cross-Batch Invariants

The repository must retain regression coverage for:

- all domain-facing public surfaces using the existing normalized live public key as the only carrier key
- every public `namespace.key` entering the namespace registry before it appears in a public surface
- the canonical `domain_object_key` grammar `dom:v1/<carrier_kind>/<encoded_carrier_key>/<subtype_namespace>/<subtype_name>`
- the v1 selector-kind set remaining limited to:
  - `page`
  - `text_excerpt`
  - `token_ref`
  - `opaque_domain_selector`
- domain state remaining derived from attachment / PDF substrate instead of becoming an independent truth source
- generic domain refs remaining outside the existing backlinks public surface by default
- generic domain refs not creating new search-hit kinds in the existing search surface by default

## Batch 1: Domain Metadata Namespace v1

The repository must retain regression coverage for:

- `kernel_query_attachment_domain_metadata(...)` succeeds for live attachment carriers
- `kernel_query_pdf_domain_metadata(...)` succeeds for live PDF carriers
- registered domain metadata keys appear in public lookup surfaces
- unregistered domain metadata keys are rejected from public surfaces
- namespace roots outside `chem.*`, `physics.*`, `bio.*`, and `generic.*` are rejected
- public metadata respects frozen value kinds
- internal-only domain metadata does not leak into public lookup surfaces
- attachment-carrier domain metadata lookup uses the existing normalized live public key
- PDF-carrier domain metadata lookup uses the existing normalized live public key
- rebuild reconstitutes the same public domain metadata for the same live carrier truth
- recovery does not preserve torn or conflicting public domain metadata values
- watcher refresh realigns domain metadata to the same truth as rebuild

## Batch 2: Domain Object Subtype Contract v1

The repository must retain regression coverage for:

- attachment -> subtype queries return canonical `domain_object_key` values
- PDF -> subtype queries return canonical `domain_object_key` values
- single subtype lookup roundtrips the same canonical `domain_object_key`
- subtype states distinguish `present`, `missing`, `unresolved`, and `unsupported`
- subtype meaning refines but does not replace coarse carrier kind
- rebuild reconstitutes the same subtype surface for the same carrier truth
- recovery realigns subtype state to the same truth as rebuild
- watcher overflow / full rescan realigns subtype state to the same truth as rebuild
- subtype diagnostics summaries use the same canonical `domain_object_key` and carrier keys as query surfaces

## Batch 3: Generic Domain Source Reference Substrate v1

The repository must retain regression coverage for:

- note -> domain refs return canonical `target_object_key` values
- domain object -> note referrers return the same canonical `target_object_key`
- `selector_kind` accepts only the frozen v1 selector set
- selector serialization roundtrips without drift
- validation states distinguish resolved, stale, unresolved, and unsupported conditions
- preview text remains plain text, single segment, and bounded
- rebuild reconstitutes the same domain source-reference surface for the same vault truth
- recovery realigns domain source references to the same truth as rebuild
- watcher overflow / full rescan realigns domain source references to the same truth as rebuild
- generic domain refs do not enter the existing backlinks public surface
- generic domain refs do not create new search-hit kinds in the existing search public surface

## Batch 4: Domain Diagnostics / Rebuild / Gates v1

The repository must retain regression coverage for:

- diagnostics export exposes `domain_contract_revision`
- diagnostics export exposes namespace, subtype, and source-reference summaries
- diagnostics export exposes last domain recount reason and timestamp
- diagnostics export exposes capability-track status summary
- rebuild updates domain recount summaries using the same public keys as query surfaces
- recovery updates domain recount summaries using the same public keys as query surfaces
- watcher overflow / full rescan updates domain recount summaries using the same public keys as query surfaces
- benchmark gates cover domain metadata lookup, subtype lookup, and domain-source-reference queries
- release checklist requires domain diagnostics smoke, domain regression matrix, and domain benchmark gates for any formal capability track
