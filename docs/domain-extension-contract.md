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
