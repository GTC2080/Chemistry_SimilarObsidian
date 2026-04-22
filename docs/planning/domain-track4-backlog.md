<!-- Reason: This file freezes the Track 4 execution order and admission rules so multi-domain substrate work lands on the stable kernel/search/attachment/PDF baseline without leaking single-discipline semantics into core truth. -->

# Domain Track 4 Backlog

Last updated: `2026-04-22`

## Scope

Track 4 opens a multi-domain-compatible kernel substrate on top of the formal attachment surface, formal PDF substrate, and existing support-bundle / gate system.

Track 4 remains inside the kernel boundary:

- domain metadata must remain namespaced and attached to existing carriers
- domain object subtypes must remain derived from attachment / PDF-adjacent truth
- generic domain source references must build on existing attachment and PDF substrate rules
- Track 4 does not add chemistry-specific deep parsing, biology-specific deep parsing, physics-specific deep parsing, UI, plugin hooks, sync, or multi-vault behavior

## Frozen Cross-Batch Rules

These four rules are fixed before implementation.

### Rule 1: Carrier Key

- all domain-facing public surfaces use the existing normalized live public key as the only carrier key
- this rule applies to:
  - domain metadata carrier lookup
  - domain object subtype lookup
  - domain source reference targets
  - diagnostics summary keys
  - support-bundle export keys
- row ids, internal metadata keys, and UI object ids must not become public carrier keys
- a capability track must not invent a carrier-key variant for one surface while using another on a different surface

### Rule 2: Namespace Registry

- every public `namespace.key` must be registered before it can enter a public surface
- the registry must freeze:
  - `namespace`
  - `key_name`
  - `public_schema_revision`
  - `value_kind`
  - `visibility`
  - `owning_capability_track`
- unregistered keys must not appear in public ABI or diagnostics contract
- the same `namespace.key` must not change meaning without a revision change

### Rule 3: Domain Object Key Grammar

- every public domain object key follows the canonical grammar:
  - `dom:v1/<carrier_kind>/<encoded_carrier_key>/<subtype_namespace>/<subtype_name>`
- `carrier_kind`, `subtype_namespace`, and `subtype_name` use lowercase ASCII only
- `encoded_carrier_key` is derived from the existing normalized live public key and then escaped for the canonical grammar
- diagnostics, query results, and source references must all reuse the same serialized form
- `domain_object_key` is a derived public key, not a new core truth id

### Rule 4: Selector Kind v1

- the only selector kinds allowed in the first public contract are:
  - `page`
  - `text_excerpt`
  - `token_ref`
  - `opaque_domain_selector`
- selector payloads must be serializable, bounded, rebuildable, and diagnosable
- viewport state, coordinates, geometry, and viewer-session data must not enter the selector contract

## Batch 1: Domain Metadata Namespace v1

Goal:

- freeze namespaced domain metadata rules so future domain capability tracks extend existing carriers without modifying core truth

Required work:

- distinguish core metadata from domain metadata
- freeze public namespace roots:
  - `chem.*`
  - `physics.*`
  - `bio.*`
  - `generic.*`
- add a formal namespace registry and visibility rules
- freeze which values may enter public surfaces and which remain capability-layer-only
- add formal attachment-carrier and PDF-carrier domain metadata lookup surfaces
- define rebuild / recovery / watcher-refresh behavior for domain metadata recomputation
- document conflict, revision, and future-extension rules

Required documents:

- `planning/domain-track4-backlog.md`
- `contracts/domain-extension-contract.md`

Acceptance:

- hosts can read stable namespaced domain metadata without touching SQLite or inferring carrier rules themselves

## Batch 2: Domain Object Subtype Contract v1

Goal:

- freeze a minimal derived subtype model so future domain objects can exist as stable attachment / PDF-adjacent public objects without changing core truth

Required work:

- add a formal derived subtype surface keyed by canonical `domain_object_key`
- freeze subtype state semantics:
  - `present`
  - `missing`
  - `unresolved`
  - `unsupported`
- freeze subtype relation to carrier kind and coarse-grained attachment kind
- add attachment -> subtype, PDF -> subtype, and single-subtype lookup surfaces
- define rebuild / recovery / watcher-refresh subtype recount rules

Required documents:

- `contracts/domain-extension-contract.md`
- `regression/domain-regression-matrix.md`

Acceptance:

- hosts can query stable domain object subtype descriptors without inventing their own identity layer

## Batch 3: Generic Domain Source Reference Substrate v1

Goal:

- expose a formal note <-> domain object reference surface that generalizes the substrate pattern without reopening search or backlinks semantics

Required work:

- add note -> domain source refs queries
- add domain object -> note referrers queries
- freeze the minimal reference shape:
  - `target_object_key`
  - `selector_kind`
  - `selector_serialized`
  - `preview_text`
  - `validation_state`
  - `target_basis_revision`
- keep generic domain refs outside the existing backlinks public surface
- keep generic domain refs outside the existing search-hit surface
- define rebuild / recovery / watcher-refresh recount rules for domain refs

Required documents:

- `contracts/domain-extension-contract.md`
- `regression/domain-regression-matrix.md`

Acceptance:

- hosts can read stable note <-> domain object references without UI selection state or ad hoc linkage logic

## Batch 4: Domain Diagnostics / Rebuild / Gates v1

Goal:

- make domain capability tracks obey the same diagnostics, rebuild, recovery, benchmark, and release-gate discipline as the rest of the kernel

Required work:

- extend diagnostics contract with domain namespace, subtype, and source-reference summaries
- freeze `domain_contract_revision`, `domain_diagnostics_revision`, and `domain_benchmark_gate_revision`
- add capability-track admission rules for formal gated status
- extend benchmark gates for domain metadata lookup, subtype lookup, and domain-source-reference queries
- extend release checklist and regression matrix for domain capability tracks
- require domain recount summaries after rebuild and watcher full rescan

Required documents:

- `contracts/domain-extension-contract.md`
- `regression/domain-regression-matrix.md`

Acceptance:

- no future domain capability can enter the repository as a special case outside diagnostics, benchmarks, regression, and release governance
