<!-- Reason: This file freezes the host-facing product compute rules that moved from Tauri Rust into the sealed kernel. -->

# Product Compute Contract

Last updated: `2026-04-27`

## Scope

This document covers stateless product compute surfaces that are not chemistry,
crystal, symmetry, search, or vault persistence surfaces.

Current surface:

- `kernel_compute_truth_diff(prev_content, prev_size, curr_content, curr_size, file_extension, out_result)`
- `kernel_free_truth_diff_result(out_result)`
- `kernel_build_semantic_context(content, content_size, out_buffer)`
- `kernel_get_semantic_context_min_bytes(out_bytes)`
- `kernel_get_rag_context_per_note_char_limit(out_chars)`
- `kernel_get_embedding_text_char_limit(out_chars)`

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
- the kernel owns semantic context trimming, heading extraction, recent-block
  selection, and context length limits
- the kernel owns host-facing AI/product text limits used for semantic context
  gating, RAG note snippets, and embedding request input trimming
- returned awards and strings are kernel-owned until released with
  `kernel_free_truth_diff_result(...)`
- returned semantic context bytes are kernel-owned until released with
  `kernel_free_buffer(...)`
- host sealed bridges may serialize truth awards and semantic context into
  host-owned JSON/text before crossing into higher-level runtimes
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
- hosts must preserve the existing Tauri command shape for
  `build_semantic_context`
- hosts must not reimplement truth diff award routing or scoring rules
- hosts must not reimplement semantic context extraction rules
- hosts must not hard-code semantic context gating, RAG note snippet, or
  embedding input text limits
- Rust hosts must not retain product compute C ABI mirror structs or unsafe
  result-copy loops for truth diff awards or semantic context buffers
- hosts may map `KERNEL_TRUTH_AWARD_REASON_*` to localized reason strings
- frontends continue to consume host commands rather than kernel ABI directly

## Semantic Context Rules

Frozen rules:

- leading and trailing whitespace is trimmed before extraction
- content at or below `2200` bytes returns the trimmed content
- long content extracts up to the last four Markdown headings shaped as
  `# `, `## `, `### `, or `#### ` after leading whitespace is ignored
- long content extracts up to the last three non-empty blocks split by blank
  lines
- heading and recent-block sections are joined with the existing labels:
  `Headings:` and `Recent focus:`
- if the joined context is at least `24` bytes, the last `2200` bytes of that
  joined context are returned
- otherwise, the last `2200` bytes of the trimmed content are returned
- null non-empty content buffers and null output buffers are invalid
- `kernel_get_semantic_context_min_bytes(...) = 24`

## AI Host Text Limits

Frozen rules:

- `kernel_get_rag_context_per_note_char_limit(...) = 1500`
- `kernel_get_embedding_text_char_limit(...) = 2000`
- null output pointers are invalid
