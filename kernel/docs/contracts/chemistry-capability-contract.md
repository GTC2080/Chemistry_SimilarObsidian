<!-- Reason: This file freezes the host-facing Track 5 invariants so chemistry spectra land as the first formal domain capability track without becoming a new truth source or leaking chemistry-specific structure into core schema. -->

# Chemistry Capability Contract

Last updated: `2026-04-24`

## Scope

This document freezes the Track 5 host-facing invariants for chemistry spectra.

It applies to:

- `chem.spectrum.*` metadata surfaces
- chemistry spectrum subtype surfaces
- note <-> chemistry spectrum source-reference surfaces
- chemistry diagnostics export keys
- chemistry support-bundle identity

Track 5 v1 is limited to chemistry spectra only.

Current exclusions:

- `chem.structure.*`
- `chem.reaction.*`
- `chem.sample.*`
- `chem.synthesis.*`
- viewer or workflow state

## Stateless Compute Surface

The polymerization kinetics simulator, stoichiometry recalculation,
spectroscopy text parser, and molecular preview builder are chemistry
compute/read surfaces, not Track 5 persisted capability surfaces.

They land:

- `kernel_simulate_polymerization_kinetics(params, out_result)`
- `kernel_free_polymerization_kinetics_result(out_result)`
- `kernel_recalculate_stoichiometry(rows, count, out_rows)`
- `kernel_parse_spectroscopy_text(raw, raw_size, extension, out_data)`
- `kernel_free_spectroscopy_data(out_data)`
- `kernel_build_molecular_preview(raw, raw_size, extension, max_atoms, out_preview)`
- `kernel_free_molecular_preview(out_preview)`

Frozen rules:

- compute surfaces are handle-free and must not read or write vault state
- compute surfaces must not create chemistry truth rows, domain objects, search hits,
  source references, diagnostics counters, or rebuild work
- the kinetics surface returns only deterministic arrays derived from the input parameter
  struct
- the stoichiometry surface writes deterministic numeric row outputs into a
  host-owned output buffer
- the spectroscopy parser returns deterministic x/series arrays and labels
  derived only from caller-provided text and extension
- the molecular preview builder returns deterministic PDB/XYZ/CIF preview text
  derived only from caller-provided text, extension, and atom limit
- Tauri Rust may own serde command marshalling, but not the simulation model
- Tauri Rust may own file IO and serde command marshalling, but not
  spectroscopy CSV/JDX parsing rules
- Tauri Rust may own file IO and serde command marshalling, but not PDB/XYZ
  molecular preview construction rules
- Tauri Rust and other hosts keep row identity, names, formulas, and UI labels;
  the kernel owns stoichiometry numeric propagation rules
- all returned kinetics arrays are kernel-owned until released with
  `kernel_free_polymerization_kinetics_result(...)`
- stoichiometry output rows remain host-owned and require no kernel free call
- all returned spectroscopy arrays, labels, and titles are kernel-owned until
  released with `kernel_free_spectroscopy_data(...)`
- returned molecular preview text is kernel-owned until released with
  `kernel_free_molecular_preview(...)`

Frozen kinetics validation bounds:

- `m0 > 0`
- `i0 >= 0`
- `cta0 >= 0`
- `kd >= 0`
- `kp >= 0`
- `kt >= 0`
- `ktr >= 0`
- `time_max > 0`
- `10 <= steps <= 50000`

Invalid inputs return `KERNEL_ERROR_INVALID_ARGUMENT` and leave the output
result empty.

Frozen stoichiometry rules:

- the first row marked `is_reference` is the reference row
- if no row is marked `is_reference`, row `0` is the reference row
- the reference row has `eq = 1`
- dependent rows derive `moles = reference_moles * eq`
- non-finite or non-positive `mw`, `eq`, or reference `moles` normalize to `0`
- explicit positive density is preserved
- missing or non-positive density may be inferred from previous positive
  `mass / volume`
- output `mass = moles * mw`
- output `volume = mass / density` when positive density is available,
  otherwise `0`

Frozen spectroscopy parser rules:

- supported extensions are `csv` and `jdx`
- CSV parsing accepts comma- or tab-delimited data rows
- CSV header rows provide the x label and series labels when present
- invalid or missing CSV y cells normalize to `0`
- CSV NMR inference uses the first 100 x values and the existing ppm-range
  heuristic
- JDX parsing accepts `##XYDATA=` and `##PEAK TABLE=` numeric pair blocks
- JDX title, x units, y units, and datatype are projected into the host-facing
  spectroscopy result
- parser failures report typed `kernel_spectroscopy_parse_error` values so
  hosts can keep localized command messages without owning parsing rules

Frozen molecular preview rules:

- supported extensions are `pdb`, `xyz`, and `cif`
- PDB preview counts `ATOM` and `HETATM` records, preserves non-atom lines, and
  emits only the first `max_atoms` atom records
- XYZ preview ignores blank atom rows, rewrites the first line to the previewed
  atom count, preserves the comment line, and emits only the first `max_atoms`
  atom rows
- CIF preview preserves raw text and does not infer atom counts through this
  surface
- unsupported extensions report `KERNEL_MOLECULAR_PREVIEW_ERROR_UNSUPPORTED_EXTENSION`

## Carrier Boundary

Track 5 must remain derived from existing kernel carriers.

Frozen rules:

- chemistry metadata attaches only to existing carriers
- chemistry spectra subtype is an attachment-carrier subtype in Track 5 v1
- Track 5 does not create a chemistry truth catalog
- rebuild, recovery, and watcher refresh must re-derive chemistry state from the same attachment truth already used by the stable kernel
- all chemistry-facing public lookups use the normalized live attachment `rel_path` as the only document key

## Batch 1 Surface

Batch 1 lands:

- `kernel_query_chem_spectrum_metadata(handle, attachment_rel_path, limit, out_entries)`
- `kernel_free_domain_metadata_list(out_entries)`

### Namespace Registry

The first public chemistry namespace root is:

- `chem.spectrum.*`

Every public chemistry key must be registered with:

- `namespace`
- `key_name`
- `public_schema_revision`
- `value_kind`
- `visibility`
- `owning_capability_track`

Frozen public keys:

- `chem.spectrum.family`
- `chem.spectrum.x_axis_unit`
- `chem.spectrum.y_axis_unit`
- `chem.spectrum.point_count`
- `chem.spectrum.source_format`
- `chem.spectrum.sample_label`

Frozen public value kinds:

- `family`
  - token
- `x_axis_unit`
  - bounded short string
- `y_axis_unit`
  - bounded short string
- `point_count`
  - uint64
- `source_format`
  - token
- `sample_label`
  - bounded short string

Frozen visibility rules:

- the six registered keys above may enter the public surface
- raw point arrays, raw headers, parser warnings, vendor-private fields, inferred peak lists, and unbounded payloads remain capability-layer-only

### `sample_label` Rule

Frozen normalization rules:

- input must decode as UTF-8
- export uses UTF-8 NFC normalization
- leading and trailing whitespace are stripped
- tabs and line breaks collapse to a single ASCII space
- empty normalized output is treated as missing
- maximum public length is `128 bytes`
- oversized values are excluded from the public surface and recorded as normalization anomalies

### Metadata Tokens

Frozen `chem.spectrum.family` tokens:

- `nmr_like`
- `ir_like`
- `uv_like`
- `ms_like`
- `unknown`

Frozen `chem.spectrum.source_format` tokens:

- `jcamp_dx`
- `spectrum_csv_v1`

### Metadata Revision

Frozen revision rule:

- `chemistry_metadata_revision = f(attachment_content_revision, chemistry_extract_mode_revision)`

Frozen boundary rule:

- metadata changes must not stale chemistry selectors unless they change `normalized_spectrum_basis`

## Batch 2 Surface

Batch 2 lands:

- `kernel_query_chem_spectra(handle, limit, out_spectra)`
- `kernel_get_chem_spectrum(handle, attachment_rel_path, out_spectrum)`
- `kernel_free_chem_spectrum_record(out_spectrum)`
- `kernel_free_chem_spectrum_list(out_spectra)`

Frozen list / lookup rules:

- `kernel_query_chem_spectra(...)` returns the chemistry spectrum candidate catalog sorted by normalized live attachment `rel_path`
- the chemistry spectrum candidate catalog includes:
  - supported `jcamp_dx` carriers
  - supported `spectrum_csv_v1` carriers
  - chemistry-like live carriers outside the Track 5 v1 support set
- non-candidate live attachments do not enter `kernel_query_chem_spectra(...)`
- `kernel_get_chem_spectrum(...)` returns `NOT_FOUND` for:
  - non-live attachment paths
  - non-candidate attachment paths

Frozen `kernel_chem_spectrum_record` fields:

- `attachment_rel_path`
  - normalized live attachment `rel_path`
- `domain_object_key`
  - canonical `dom:v1/attachment/<encoded_rel_path>/chem/spectrum`
- `subtype_revision`
  - `1`
- `source_format`
  - `jcamp_dx`
  - `spectrum_csv_v1`
  - `unknown` for unsupported chemistry-like carriers
- `coarse_kind`
  - copied from the stable attachment truth surface
- `presence`
  - copied from the stable attachment truth surface
- `state`
  - derived chemistry subtype state
- `flags`
  - `none` in Track 5 v1

### Supported Formats

Frozen Track 5 v1 support set:

- `jcamp_dx`
- `spectrum_csv_v1`

No other chemistry attachment format is public in Track 5 v1.

### `spectrum_csv_v1` Contract

Frozen contract:

- encoding: `UTF-8`
- delimiter: `,`
- exact header row: `x,y`
- comment prefix: `#`
- only allowed pre-header comment keys:
  - `# x_unit=<unit>`
  - `# y_unit=<unit>`
  - `# family=<token>`
  - `# sample_label=<text>`
- the first non-comment, non-empty line must be `x,y`
- every data row must contain exactly two fields in `x,y` order
- empty lines are forbidden after the header
- unit values must come from the comment block
- files outside this contract degrade to `unresolved`

### Subtype Identity

Frozen subtype namespace and name:

- `subtype_namespace = chem`
- `subtype_name = spectrum`

Frozen `domain_object_key` grammar:

- `dom:v1/attachment/<encoded_rel_path>/chem/spectrum`

Frozen state set:

- `present`
- `missing`
- `unresolved`
- `unsupported`

Frozen state rules:

- supported format + stable parse + present file -> `present`
- live attachment ref with missing file -> `missing`
- supported format with unstable parse / required-field conflict -> `unresolved`
- live attachment outside Track 5 v1 support set -> `unsupported`

Frozen `source_format` mapping:

- `.jdx` / `.dx` -> `jcamp_dx`
- strict Track 5 CSV contract -> `spectrum_csv_v1`
- unsupported chemistry-like carriers -> `unknown`

## Batch 3 Surface

Batch 3 lands:

- `kernel_query_note_chem_spectrum_refs(handle, note_rel_path, limit, out_refs)`
- `kernel_query_chem_spectrum_referrers(handle, attachment_rel_path, limit, out_referrers)`
- corresponding free functions for chemistry spectrum refs / referrers

### Source-Reference Boundary

Frozen rules:

- chemistry refs build on the existing generic domain source-reference substrate
- chemistry refs do not create a new truth source
- chemistry refs do not enter the existing backlinks public surface
- chemistry refs do not create a new search-hit kind
- chemistry refs do not alter PDF reference public semantics

### Selector Set

Frozen Track 5 v1 selector kinds:

- `whole_spectrum`
- `x_range`

Track 5 v1 excludes:

- peak tokens
- geometry selectors
- UI selection state

### Selector Grammar

Frozen serialized forms:

- `chemsel:v1|kind=whole|basis=<chemistry_selector_basis_revision>`
- `chemsel:v1|kind=x_range|basis=<chemistry_selector_basis_revision>|start=<normalized_decimal>|end=<normalized_decimal>|unit=<normalized_unit>`

Frozen `normalized_decimal` grammar:

- `-?(0|[1-9][0-9]*)(\\.[0-9]+)?`

Frozen decimal normalization rules:

- scientific notation is forbidden
- `+` prefixes are forbidden
- `-0` normalizes to `0`
- trailing fractional zeros are removed
- a now-empty fractional part removes the decimal point

Frozen `normalized_unit` grammar:

- `[a-z][a-z0-9./-]*`

Frozen unit rules:

- units use lowercase ASCII canonical tokens
- aliases must explicitly fold to canonical tokens
- undeclared aliases are not public

### Selector Basis Revision

Frozen revision rule:

- `chemistry_selector_basis_revision = f(attachment_content_revision, chemistry_selector_mode_revision, normalized_spectrum_basis)`

Frozen boundary rule:

- metadata-only changes do not stale selectors unless they change `normalized_spectrum_basis`

## Batch 4 Diagnostics Contract

Batch 4 lands:

- diagnostics export extends the support bundle with chemistry revisions, recount markers, summaries, and counts
- benchmark gates extend the query/rebuild benchmark suite with formal chemistry timings

Frozen diagnostics keys:

- `chemistry_contract_revision`
- `chemistry_diagnostics_revision`
- `chemistry_benchmark_gate_revision`
- `chemistry_namespace_summary`
- `chemistry_spectra_subtype_summary`
- `chemistry_spectra_source_reference_summary`
- `chemistry_spectra_count`
- `chemistry_spectra_present_count`
- `chemistry_spectra_missing_count`
- `chemistry_spectra_unresolved_count`
- `chemistry_spectra_unsupported_count`
- `chemistry_source_ref_count`
- `chemistry_source_ref_stale_count`
- `last_chemistry_recount_reason`
- `last_chemistry_recount_at_ns`
- `chemistry_capability_track_status_summary`

Frozen chemistry capability status values:

- `contract_only`
- `gated`
- `blocked`

## Lifecycle Consistency Rules

Frozen rules:

- rebuild must recompute chemistry metadata, subtype, and refs from live attachment truth
- recovery must discard torn partial chemistry state and re-derive from disk truth
- watcher refresh must only update affected chemistry carrier-derived state
- watcher overflow and full rescan must realign chemistry state to the same truth as rebuild
- chemistry diagnostics and support-bundle exports must use the same normalized live keys and revisions as chemistry query surfaces

## Admission Rules

Track 5 becomes formally gated only when it lands:

- registered `chem.spectrum.*` public keys
- frozen chemistry subtype ABI
- frozen chemistry source-reference ABI
- diagnostics keys and summaries
- regression-matrix entries
- benchmark baselines and thresholds
- release-checklist updates

A chemistry capability slice that is not `gated` must not present itself as a formal public surface.
