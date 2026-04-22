<!-- Reason: This file freezes the host-facing Track 4 invariants so future domain capability tracks extend attachment and PDF substrate through one stable domain contract instead of introducing single-discipline truth models. -->

# Domain Extension Contract

Last updated: `2026-04-22`

## Scope

This document freezes the host-facing invariants that all Track 4 domain-extension surfaces must obey.

It applies to:

- domain metadata namespace surfaces
- derived domain object subtype surfaces
- future generic domain source-reference surfaces
- domain diagnostics export keys
- support-bundle domain identity

Batch 1 lands:

- `kernel_query_attachment_domain_metadata(handle, attachment_rel_path, limit, out_entries)`
- `kernel_query_pdf_domain_metadata(handle, attachment_rel_path, limit, out_entries)`
- `kernel_free_domain_metadata_list(out_entries)`

Batch 2 lands:

- `kernel_query_attachment_domain_objects(handle, attachment_rel_path, limit, out_objects)`
- `kernel_query_pdf_domain_objects(handle, attachment_rel_path, limit, out_objects)`
- `kernel_get_domain_object(handle, domain_object_key, out_object)`
- `kernel_free_domain_object_descriptor(out_object)`
- `kernel_free_domain_object_list(out_objects)`

Batch 3 lands:

- `kernel_query_note_domain_source_refs(handle, note_rel_path, limit, out_refs)`
- `kernel_query_domain_object_referrers(handle, domain_object_key, limit, out_referrers)`
- `kernel_free_domain_source_refs(out_refs)`
- `kernel_free_domain_referrers(out_referrers)`

Batch 4 lands:

- diagnostics export extends the support bundle with domain contract revisions, recount summaries, and domain-count summaries
- benchmark gates extend the query benchmark with formal domain metadata, object, and source-reference timings

## Carrier Boundary

Domain capability tracks remain derived from existing kernel carriers.

Frozen boundary rules:

- domain metadata may attach only to existing carriers
- the first public carriers are:
  - attachment carrier
  - PDF carrier
  - domain source-reference carrier once Batch 3 lands
- domain state does not create an independent truth catalog
- rebuild, recovery, and watcher refresh must re-derive domain state from the same disk truth already used by attachment and PDF substrate
- a non-live carrier key is not a valid public domain lookup key

## Frozen Carrier Key Rule

All domain-facing public lookup surfaces use:

- the existing normalized live public key for the carrier

as the only public carrier key.

This rule is frozen for:

- domain metadata lookup
- subtype lookup
- note -> domain refs `target_object_key` basis
- diagnostics summary keys
- support-bundle export keys

Public carrier identity must not be:

- a storage row id
- an internal metadata row key
- a UI object id
- a viewer or workflow session id

## Namespace Registry

Every public namespace entry must be registered before it can appear in public ABI, diagnostics, or support-bundle output.

The registry freezes:

- `namespace`
- `key_name`
- `public_schema_revision`
- `value_kind`
- `visibility`
- `owning_capability_track`

Frozen namespace roots:

- `chem.*`
- `physics.*`
- `bio.*`
- `generic.*`

Frozen semantics:

- namespace roots and key names use lowercase ASCII only
- the same `namespace.key` must not change meaning without a `public_schema_revision` change
- unregistered namespace entries must not enter public surfaces
- internal-only keys may exist in implementation state but must not leak into host-facing contracts

Current public registry entries:

- attachment carrier:
  - `generic.carrier_surface`
  - `generic.coarse_kind`
  - `generic.presence`
- PDF carrier:
  - `generic.carrier_surface`
  - `generic.doc_title_state`
  - `generic.has_outline`
  - `generic.metadata_state`
  - `generic.page_count`
  - `generic.presence`
  - `generic.text_layer_state`

Current registry exclusions:

- no `chem.*` public keys yet
- no `physics.*` public keys yet
- no `bio.*` public keys yet

## Public Metadata Rules

Only stable, bounded, rebuildable metadata may enter a public surface.

Allowed public value kinds:

- boolean
- signed integer
- unsigned integer
- enum token
- bounded short string

Excluded from public metadata:

- parser debug payloads
- unbounded blobs
- raw extractor traces
- viewer state
- host-session state

Domain metadata remains distinct from core metadata.

Frozen semantics:

- core metadata continues to define existing kernel truth and existing public surfaces
- domain metadata is namespaced, derived, and optional
- domain metadata may refine carrier interpretation but must not rewrite carrier identity or core truth

## Frozen Domain Object Key Grammar

Every public domain object key uses the canonical serialized grammar:

- `dom:v1/<carrier_kind>/<encoded_carrier_key>/<subtype_namespace>/<subtype_name>`

Frozen semantics:

- `carrier_kind`
  - lowercase ASCII token naming the existing carrier family
- `encoded_carrier_key`
  - derived from the existing normalized live public key and escaped for the canonical grammar
- `subtype_namespace`
  - lowercase ASCII namespace root such as `chem`, `physics`, `bio`, or `generic`
- `subtype_name`
  - lowercase ASCII subtype token

The same serialized form must be used by:

- subtype queries
- domain source-reference targets
- diagnostics
- support-bundle exports

`domain_object_key` is a derived public key, not a new core truth id.

Frozen escape rules:

- `encoded_carrier_key` uses percent-encoding for any byte outside the unreserved set
- the unreserved set is `A-Z`, `a-z`, `0-9`, `-`, `.`, `_`, `~`
- `/` in normalized carrier keys must serialize as `%2F`
- the canonical serialized form uses uppercase hex digits in escape sequences
- non-canonical serialized keys are rejected by single-object lookup

## Domain Object Subtype Rules

Domain object subtypes are derived views over existing carriers.

Frozen subtype states:

- `present`
- `missing`
- `unresolved`
- `unsupported`

Frozen subtype rules:

- a subtype must remain attached to an existing carrier
- subtype meaning may refine a coarse attachment or PDF classification but must not replace it
- a subtype may become `missing` when the carrier remains live but the underlying file truth is missing
- a subtype may become `unresolved` when the carrier exists but the domain contract cannot derive stable subtype state
- a subtype may become `unsupported` when the namespace and subtype are known but the current kernel mode does not expose a stable surface for it

Current public subtype entries:

- attachment carrier:
  - `generic.attachment_resource`
- PDF carrier:
  - `generic.pdf_document`

Current public subtype revisions:

- `generic.attachment_resource`
  - `subtype_revision = 1`
- `generic.pdf_document`
  - `subtype_revision = 1`

Current public state mapping:

- `generic.attachment_resource`
  - present carrier -> `present`
  - missing carrier -> `missing`
- `generic.pdf_document`
  - ready / partial PDF metadata -> `present`
  - missing PDF carrier -> `missing`
  - invalid / unavailable PDF metadata -> `unresolved`

Current exclusions:

- no public subtype currently returns `unsupported`
- no discipline-specific subtype is public yet

## Frozen Selector Kind Rule

The first public generic domain source-reference contract allows only:

- `page`
- `text_excerpt`
- `token_ref`
- `opaque_domain_selector`

Frozen semantics:

- selector payloads must be serializable, bounded, rebuildable, and diagnosable
- selector payloads must not contain viewport state, screen coordinates, geometry, highlight styling, or viewer-session state
- `preview_text` remains plain text, single segment, and bounded in length

Current Batch 3 emission rules:

- current generic domain refs emit only:
  - `selector_kind = opaque_domain_selector`
- the current generic domain substrate projects the formal PDF anchor string into:
  - `selector_serialized`
- `target_basis_revision` is filled from the parsed PDF anchor basis revision when selector parsing succeeds
- malformed selectors keep the stored selector string and downgrade `target_basis_revision` to empty

Current public source-reference target set:

- `generic.pdf_document`
  - participates in the generic domain source-reference surface
- `generic.attachment_resource`
  - currently returns an empty generic referrer list

Current generic reference-state mapping:

- projected resolved PDF refs -> `resolved`
- projected missing PDF refs -> `missing`
- projected stale PDF refs -> `stale`
- projected unresolved PDF refs -> `unresolved`
- no current public generic ref emits `unsupported`

## Surface Consistency Rules

Track 4 must not reopen or silently rewrite existing public surfaces.

Frozen consistency rules:

- attachment public surface remains the host-facing attachment truth surface
- PDF public surface remains the host-facing PDF substrate surface
- domain metadata, subtype, and source references extend those surfaces; they do not replace them
- generic domain refs do not enter the existing backlinks public surface by default
- generic domain refs do not create new hit kinds in the existing search public surface by default

## Lifecycle Consistency Rules

All domain state must follow existing kernel lifecycle discipline.

Frozen lifecycle rules:

- rebuild must recompute domain state from live carrier truth
- recovery must clear torn partial domain state and re-derive from disk truth
- watcher refresh must only update affected carrier-derived domain state
- watcher overflow and full rescan must realign domain state to the same truth as rebuild
- diagnostics and support-bundle exports must describe current and recent domain recount outcomes using the same public keys and revisions as query surfaces

## Current Diagnostics Contract

Current diagnostics export keys:

- `last_domain_recount_reason`
- `last_domain_recount_at_ns`
- `domain_contract_revision`
- `domain_diagnostics_revision`
- `domain_benchmark_gate_revision`
- `domain_namespace_summary`
- `domain_subtype_summary`
- `domain_source_reference_summary`
- `domain_attachment_metadata_entry_count`
- `domain_pdf_metadata_entry_count`
- `domain_object_count`
- `domain_source_ref_count`
- `domain_source_ref_resolved_count`
- `domain_source_ref_missing_count`
- `domain_source_ref_stale_count`
- `domain_source_ref_unresolved_count`
- `domain_source_ref_unsupported_count`
- `domain_unresolved_summary`
- `domain_stale_summary`
- `domain_unsupported_summary`
- `capability_track_status_summary`

Current frozen revision values:

- `domain_contract_revision = track4_batch3_domain_extension_contract_v1`
- `domain_diagnostics_revision = track4_batch4_domain_diagnostics_v1`
- `domain_benchmark_gate_revision = track4_batch4_domain_query_gates_v1`

Current summary semantics:

- `domain_namespace_summary`
  - the comma-separated public namespace roots currently exposed by the formal domain surface
- `domain_subtype_summary`
  - the comma-separated public subtype identifiers currently exposed by the formal domain surface
- `domain_source_reference_summary`
  - the current generic source-reference substrate projection set
- `domain_unresolved_summary`
  - `clean` when no unresolved public domain objects or refs are present
  - `domain_source_refs` when unresolved projected generic refs are present
- `domain_stale_summary`
  - `clean` when no stale projected generic refs are present
  - `domain_source_refs` when stale projected generic refs are present
- `domain_unsupported_summary`
  - `clean` when no unsupported public domain surface is active
- `capability_track_status_summary`
  - the semicolon-separated status summary for currently formalized domain capability slices

## Current Benchmark Gate Set

Current formal domain query gates:

- `domain_attachment_metadata_query`
- `domain_pdf_metadata_query`
- `domain_attachment_objects_query`
- `domain_pdf_objects_query`
- `domain_object_lookup_query`
- `domain_note_source_refs_query`
- `domain_object_referrers_query`

## Capability-Track Admission Rules

No domain capability track becomes formal and gated unless it lands:

- registered namespace entries
- a frozen subtype contract if it exposes objects
- a frozen source-reference contract if it exposes note references
- diagnostics keys and summaries
- regression-matrix entries
- benchmark baselines and thresholds
- release-checklist updates

Frozen capability-track status values:

- `contract_only`
- `gated`
- `blocked`

A capability track that is not `gated` must not present itself as a formal public surface.
