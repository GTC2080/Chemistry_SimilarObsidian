<!-- Reason: This file freezes the Track 5 regression obligations and adjacent stateless chemistry compute checks so chemistry grows through explicit repository rules instead of one-off exceptions. -->

# Chemistry Regression Matrix

Last updated: `2026-04-27`

## Frozen Cross-Batch Invariants

The repository must retain regression coverage for:

- all chemistry-facing public surfaces using normalized live attachment `rel_path` as the only document key
- `chem.spectrum.*` keys entering the public surface only through the formal namespace registry
- `sample_label` following the frozen normalization and length rules
- `spectrum_csv_v1` accepting only the strict Track 5 v1 CSV contract
- `domain_object_key` for chemistry spectra remaining:
  - `dom:v1/attachment/<encoded_rel_path>/chem/spectrum`
- chemistry state remaining derived from attachment truth instead of becoming a new truth source
- chemistry refs remaining outside the existing backlinks public surface
- chemistry refs not creating new search-hit kinds in the existing search surface
- metadata-only changes not staling chemistry selectors unless `normalized_spectrum_basis` changes
- stateless chemistry compute surfaces not reading or writing vault truth

## Stateless Chemistry Compute Surface

The repository must retain regression coverage for:

- `kernel_simulate_polymerization_kinetics(...)` succeeds for valid inputs
- result arrays all have `steps + 1` samples
- result arrays are finite
- `time[0] == 0` and the final time equals `time_max`
- conversion remains bounded and increases for the baseline polymerization case
- `pdi` remains physically bounded at `>= 1`
- invalid physical parameters return `KERNEL_ERROR_INVALID_ARGUMENT`
- invalid calls clear stale kernel-owned output before returning
- `kernel_free_polymerization_kinetics_result(...)` is idempotent and leaves the result empty
- Tauri sealed bridge serializes polymerization-kinetics kernel results to JSON
  without retaining Rust-owned kinetics C ABI structs or result-copy loops
- `kernel_recalculate_stoichiometry(...)` uses the first marked reference row
- `kernel_recalculate_stoichiometry(...)` defaults row `0` as reference when no
  row is marked
- stoichiometry reference rows force `eq = 1`
- stoichiometry dependent rows derive moles, mass, and volume from the
  reference row
- stoichiometry clamps non-finite or non-positive `mw`, `eq`, and reference
  `moles`
- stoichiometry preserves explicit positive density and infers missing density
  from previous positive `mass / volume`
- stoichiometry rejects null input or output buffers when `count > 0`
- stoichiometry accepts zero-count null buffers without allocation
- Tauri `recalculate_stoichiometry` delegates empty row lists to
  `kernel_recalculate_stoichiometry(...)`
- Tauri sealed bridge serializes stoichiometry kernel outputs to JSON without
  retaining Rust-owned stoichiometry C ABI structs or output-copy loops
- `kernel_generate_mock_retrosynthesis(...)` emits an amide pathway for
  `C(=O)N` targets
- retrosynthesis target ids use the `retro_` prefix
- retrosynthesis preserves precursor roles
- retrosynthesis clamps depth to the frozen `1..4` range
- retrosynthesis rejects null or empty targets
- Tauri `retrosynthesize_target` delegates SMILES whitespace normalization and
  empty-target validation to `kernel_generate_mock_retrosynthesis(...)`
- Tauri sealed bridge serializes retrosynthesis kernel results to JSON without
  retaining Rust-owned retrosynthesis C ABI structs or result-copy loops
- `kernel_free_retro_tree(...)` leaves the tree empty and is safe on filled
  output
- `kernel_parse_spectroscopy_text(...)` parses CSV x values and multiple y
  series
- spectroscopy CSV parsing preserves header-derived x and series labels
- spectroscopy CSV parsing normalizes invalid or missing y cells to `0`
- `kernel_parse_spectroscopy_text(...)` parses JDX `##PEAK TABLE=` / numeric
  pair blocks
- spectroscopy JDX parsing preserves title, x label, y label, and NMR inference
- spectroscopy parser failures report typed parse errors without allocating
  stale result buffers
- Tauri `parse_spectroscopy` delegates extension support decisions to
  `kernel_parse_spectroscopy_text(...)`
- Tauri sealed bridge serializes spectroscopy kernel results to JSON without
  retaining Rust-owned spectroscopy C ABI structs or result-copy loops
- `kernel_free_spectroscopy_data(...)` leaves the result empty and is safe on
  partially filled data
- `kernel_build_molecular_preview(...)` counts and truncates PDB `ATOM` /
  `HETATM` records while preserving non-atom lines
- `kernel_normalize_molecular_preview_atom_limit(...)` owns default, minimum,
  and maximum preview atom bounds
- molecular preview rewrites XYZ atom counts after blank-row filtering
- molecular preview preserves CIF text without inferring atom counts
- unsupported molecular preview extensions report
  `KERNEL_MOLECULAR_PREVIEW_ERROR_UNSUPPORTED_EXTENSION`
- Tauri `read_molecular_preview` delegates extension support decisions to
  `kernel_build_molecular_preview(...)`
- Tauri `read_molecular_preview` delegates atom limit normalization to
  `kernel_normalize_molecular_preview_atom_limit(...)`
- Tauri sealed bridge serializes molecular-preview kernel results to JSON
  without retaining Rust-owned molecular-preview C ABI structs
- `kernel_free_molecular_preview(...)` leaves the result empty and is safe on
  partially filled data

## [Batch 1] Chemistry Metadata Namespace v1

The repository must retain regression coverage for:

- `kernel_query_chem_spectrum_metadata(...)` succeeds for live supported spectrum carriers
- only registered `chem.spectrum.*` keys appear in the public surface
- unsupported chemistry attachments do not leak `chem.spectrum.*` public keys
- `sample_label` normalization trims, folds whitespace, and drops empty results
- oversized `sample_label` values are excluded and reported as anomalies
- `family` and `source_format` use only frozen token sets
- required metadata conflicts degrade to non-`present` behavior
- rebuild reconstitutes the same public metadata for the same live spectrum truth
- recovery reconstitutes the same public metadata for the same live spectrum truth
- watcher refresh realigns chemistry metadata to the same truth as rebuild

## [Batch 2] Chemistry Spectra Subtype Contract v1

The repository must retain regression coverage for:

- `kernel_query_chem_spectra(...)` succeeds for live chemistry spectrum carriers
- `kernel_get_chem_spectra_default_limit(...)` returns the frozen host default
  and rejects null output pointers
- `kernel_get_chem_spectrum(...)` succeeds for supported live spectrum carriers
- Tauri sealed bridge `sealed_kernel_query_chem_spectra` and
  `sealed_kernel_get_chem_spectrum` expose the kernel catalog / lookup without
  rebuilding chemistry spectrum candidate rules in Rust
- `jcamp_dx` can enter `present`
- strict `spectrum_csv_v1` can enter `present`
- non-conforming CSV degrades to `unresolved`
- unsupported chemistry-like attachments degrade to `unsupported`
- missing live spectrum carriers degrade to `missing`
- chemistry spectrum `domain_object_key` roundtrips canonically
- subtype state does not alter attachment truth identity
- rebuild reconstitutes the same subtype surface for the same live truth
- recovery realigns subtype state to the same truth as rebuild
- watcher overflow / full rescan realigns subtype state to the same truth as rebuild

## [Batch 3] Chemistry Spectra Source Reference Public Surface v1

The repository must retain regression coverage for:

- `kernel_query_note_chem_spectrum_refs(...)` succeeds for live notes
- `kernel_query_chem_spectrum_referrers(...)` succeeds for live spectrum objects
- chemistry spectrum source-ref / referrer default limits come from kernel
  getters and reject null output pointers
- Tauri sealed bridge `sealed_kernel_query_note_chem_spectrum_refs` and
  `sealed_kernel_query_chem_spectrum_referrers` expose kernel source refs /
  referrers without rebuilding chemistry references from note text,
  attachment refs, backlinks, or search results in Rust
- whole-spectrum refs serialize canonically
- x-range refs serialize canonically with the frozen decimal/unit grammar
- `normalized_decimal` rejects scientific notation and non-canonical forms
- `normalized_unit` rejects undeclared aliases
- chemistry ref states distinguish:
  - `resolved`
  - `missing`
  - `stale`
  - `unresolved`
  - `unsupported`
- metadata-only changes do not stale refs unless `normalized_spectrum_basis` changes
- rebuild reconstitutes the same chemistry ref surface for the same vault truth
- recovery realigns chemistry refs to the same truth as rebuild
- watcher overflow / full rescan realigns chemistry refs to the same truth as rebuild
- chemistry refs do not enter the existing backlinks surface
- chemistry refs do not create new search-hit kinds
- chemistry refs do not alter PDF reference public behavior

## [Batch 4] Chemistry Diagnostics / Rebuild / Gates v1

The repository must retain regression coverage for:

- diagnostics export exposes:
  - `chemistry_contract_revision`
  - `chemistry_diagnostics_revision`
  - `chemistry_benchmark_gate_revision`
- diagnostics export exposes chemistry namespace, subtype, and source-reference summaries
- diagnostics export exposes chemistry unresolved / stale / unsupported counts
- diagnostics export exposes `last_chemistry_recount_reason` and `last_chemistry_recount_at_ns`
- rebuild updates chemistry recount summaries using the same normalized live keys as chemistry query surfaces
- recovery updates chemistry recount summaries using the same normalized live keys as chemistry query surfaces
- watcher overflow / full rescan updates chemistry recount summaries using the same normalized live keys as chemistry query surfaces
- benchmark gates cover:
  - `chemistry_metadata_query`
  - `chemistry_spectrum_catalog_query`
  - `chemistry_spectrum_lookup_query`
  - `chemistry_note_spectrum_refs_query`
  - `chemistry_spectrum_referrers_query`
  - `chemistry_rebuild_mixed_spectra_dataset`
- release checklist requires chemistry contract review, chemistry regression matrix review, chemistry diagnostics smoke, and chemistry benchmark gates before Track 5 can be called formal
- chemistry capability status is `gated` only when all required governance surfaces are present and green
