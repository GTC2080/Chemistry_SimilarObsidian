<!-- Reason: This file freezes the host-facing product compute rules that moved from Tauri Rust into the sealed kernel. -->

# Product Compute Contract

Last updated: `2026-04-25`

## Scope

This document covers stateless product compute surfaces that are not chemistry,
crystal, symmetry, search, or vault persistence surfaces.

Current surface:

- `kernel_compute_truth_diff(prev_content, prev_size, curr_content, curr_size, file_extension, out_result)`
- `kernel_free_truth_diff_result(out_result)`

Current exclusions:

- vault reads or writes
- database writes
- UI state
- localized display text

## Boundary

Frozen rules:

- the surface is handle-free and must not read or write vault state
- Tauri Rust owns serde command marshalling and localized reason text
- the kernel owns award attribute routing, award amounts, code-fence language
  detection, and molecular line-growth detection
- returned awards and strings are kernel-owned until released with
  `kernel_free_truth_diff_result(...)`
- `reason` is a typed enum; hosts map it to localized text
- `detail` is only populated for code-language awards and contains the detected
  language
- empty previous or current content returns an empty award list
- null non-empty content buffers, null extension, and null output pointers are
  invalid

## Truth Diff Rules

Frozen rules:

- text delta awards apply only when current content is more than `10` bytes
  longer than previous content
- text delta amount is `floor((delta_bytes / 100) * 2)`
- text delta attribute routing by extension:
  - `jdx`, `csv` -> `science`
  - `py`, `js`, `ts`, `tsx`, `jsx`, `rs`, `go`, `c`, `cpp`, `java` -> `engineering`
  - `mol`, `chemdraw` -> `creation`
  - `dashboard`, `base` -> `finance`
  - everything else -> `creation`
- code-fence languages are read from Markdown opening fences with three
  backticks followed immediately by ASCII word characters
- only newly introduced languages receive an award
- code-language amount is `8`
- code-language attribute routing:
  - `python`, `py`, `rust`, `go`, `javascript`, `js`, `typescript`, `ts`,
    `java`, `c`, `cpp` -> `engineering`
  - `smiles`, `chemical`, `latex`, `math` -> `science`
  - `sql`, `r`, `stata` -> `finance`
- molecular line-growth awards apply only to `mol` and `chemdraw`
- molecular line-growth amount is `added_lines * 5`
- molecular line-growth attribute is `creation`

## Host Contract

Frozen rules:

- hosts must preserve the existing Tauri command shape for `compute_truth_diff`
- hosts must not reimplement truth diff award routing or scoring rules
- hosts may map `KERNEL_TRUTH_AWARD_REASON_*` to localized reason strings
- frontends continue to consume host commands rather than kernel ABI directly
