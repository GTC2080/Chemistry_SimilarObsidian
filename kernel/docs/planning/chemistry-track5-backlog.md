<!-- Reason: This file freezes the Track 5 execution order so the first chemistry capability line lands on the stable kernel/domain substrate without reopening core truth, search, attachment, or PDF contracts. -->

# Chemistry Track 5 Backlog

Last updated: `2026-04-22`

## Scope

Track 5 opens the first formal chemistry capability track on top of the stable kernel baseline, formal attachment surface, formal PDF substrate, and formal domain extension substrate.

Track 5 is strictly limited to:

- `chem.spectrum.*` metadata
- chemistry spectrum subtype
- note <-> chemistry spectrum source references
- chemistry diagnostics, recount, benchmarks, and release gates

Track 5 does not add:

- chemistry viewer UI
- structure editor
- reaction editor
- workflow shell
- AI interpretation
- chemistry-wide object families beyond spectra-first scope

## Frozen Cross-Batch Rules

These four rules are fixed before implementation.

### Rule 1: `sample_label` Normalization

- `chem.spectrum.sample_label` is the only optional free-text field allowed in Track 5 v1 public metadata
- public `sample_label` values must be:
  - UTF-8
  - NFC-normalized
  - single-line
  - trimmed of leading and trailing whitespace
- tabs and line breaks must collapse to a single ASCII space before export
- an empty normalized value is treated as missing and must not be exported as an empty string
- the maximum exported length is `128 bytes`
- values exceeding the limit must be excluded from the public surface and recorded in diagnostics as a normalization / truncation anomaly

### Rule 2: `spectrum_csv_v1` Strict Contract

- Track 5 v1 supports only:
  - `jcamp_dx`
  - `spectrum_csv_v1`
- `spectrum_csv_v1` uses:
  - UTF-8 encoding
  - comma `,` delimiter
  - exact header row `x,y`
- optional pre-header comment lines must begin with `#`
- the only allowed comment keys are:
  - `# x_unit=<unit>`
  - `# y_unit=<unit>`
  - `# family=<token>`
  - `# sample_label=<text>`
- the first non-comment, non-empty line must be `x,y`
- each data row must contain exactly two columns in `x,y` order
- empty lines are allowed only before the header
- `x_unit` and `y_unit` must come from the comment block; Track 5 v1 does not infer them from data rows
- CSV files outside this contract must not enter `present`; they must degrade to `unresolved`

### Rule 3: Selector Grammar

- `normalized_decimal` uses canonical decimal text only
- scientific notation is forbidden
- the grammar is:
  - `-?(0|[1-9][0-9]*)(\\.[0-9]+)?`
- `+` prefixes are forbidden
- `-0` must normalize to `0`
- trailing fractional zeros must be stripped
- `normalized_unit` uses lowercase ASCII canonical tokens only
- the grammar is:
  - `[a-z][a-z0-9./-]*`
- unit aliases must be explicitly folded to canonical tokens; undeclared aliases must not enter public selector payloads

### Rule 4: Revision Boundary

- `chemistry_metadata_revision = f(attachment_content_revision, chemistry_extract_mode_revision)`
- `chemistry_selector_basis_revision = f(attachment_content_revision, chemistry_selector_mode_revision, normalized_spectrum_basis)`
- metadata changes must not automatically stale chemistry selectors unless `normalized_spectrum_basis` changes
- `sample_label`, diagnostics-only values, and other metadata-only fields must not independently stale:
  - whole-spectrum refs
  - x-range refs

## [Batch 1] Chemistry Metadata Namespace v1

Goal:

- freeze the `chem.spectrum.*` namespace as the first formal chemistry metadata surface

Required work:

- register `chem.spectrum.*` public keys and revisions
- freeze public metadata keys:
  - `chem.spectrum.family`
  - `chem.spectrum.x_axis_unit`
  - `chem.spectrum.y_axis_unit`
  - `chem.spectrum.point_count`
  - `chem.spectrum.source_format`
  - `chem.spectrum.sample_label`
- freeze value-kind, visibility, normalization, and conflict rules
- add a formal chemistry metadata query surface using normalized live attachment `rel_path`
- define rebuild / recovery / watcher-refresh recomputation rules

Required documents:

- `planning/chemistry-track5-backlog.md`
- `contracts/chemistry-capability-contract.md`

Acceptance:

- hosts can read stable `chem.spectrum.*` metadata without reading raw attachment state or internal capability-layer payloads

## [Batch 2] Chemistry Spectra Subtype Contract v1

Goal:

- freeze the first formal chemistry subtype as a spectrum object carried by the existing attachment truth surface

Required work:

- define the `chem.spectrum` subtype
- freeze supported formats:
  - `jcamp_dx`
  - `spectrum_csv_v1`
- freeze subtype states:
  - `present`
  - `missing`
  - `unresolved`
  - `unsupported`
- add spectrum list / lookup public surfaces
- define subtype identity, `domain_object_key`, and recount rules under rebuild / recovery / watcher lifecycle

Required documents:

- `contracts/chemistry-capability-contract.md`
- `regression/chemistry-regression-matrix.md`

Acceptance:

- hosts can query formal chemistry spectrum objects without inventing their own chemistry identity layer

## [Batch 3] Chemistry Spectra Source Reference Public Surface v1

Goal:

- expose formal note <-> chemistry spectrum references on top of the existing generic domain source-reference substrate

Required work:

- add note -> chemistry spectrum refs
- add chemistry spectrum -> note referrers
- freeze v1 selector kinds:
  - whole spectrum
  - x-range window
- freeze selector serialization, preview rules, validation-state rules, and recount behavior
- keep chemistry refs outside existing backlinks and existing search-hit kinds

Required documents:

- `contracts/chemistry-capability-contract.md`
- `regression/chemistry-regression-matrix.md`

Acceptance:

- hosts can read stable note <-> chemistry spectrum references without UI selection state or ad hoc note parsing

## [Batch 4] Chemistry Diagnostics / Rebuild / Gates v1

Goal:

- make the chemistry spectra line a formal gated capability track instead of an experimental feature slice

Required work:

- extend diagnostics export with chemistry revisions, summaries, counts, and recount markers
- extend benchmark gates with chemistry metadata, subtype, and source-reference query loops
- add a chemistry regression matrix
- extend release-checklist expectations for a formal chemistry capability track
- freeze chemistry capability-track admission and status rules

Required documents:

- `contracts/chemistry-capability-contract.md`
- `regression/chemistry-regression-matrix.md`

Acceptance:

- chemistry spectra can only be considered formal when diagnostics, regression, benchmarks, and checklist gates are all present and green
