<!-- Reason: This file records regression obligations for product compute rules as they move from Tauri Rust into the kernel. -->

# Product Compute Regression Matrix

Last updated: `2026-04-27`

## Truth Diff Awards

The repository must retain regression coverage for:

- `kernel_compute_truth_diff(...)` returns no awards for empty previous content
- `kernel_compute_truth_diff(...)` rejects null output pointers
- `kernel_compute_truth_diff(...)` rejects null non-empty content buffers
- `kernel_compute_truth_diff(...)` rejects null file extensions
- text delta awards preserve the legacy byte-delta floor formula
- text delta awards route `csv` and `jdx` edits to `science`
- default text delta routing remains `creation`
- newly introduced Markdown code-fence languages receive one code award
- previously existing Markdown code-fence languages do not receive duplicate
  awards
- code-language routing sends engineering languages to `engineering`
- code-language routing sends science languages to `science`
- code-language routing sends finance languages to `finance`
- unsupported code-fence languages do not receive awards
- code-language award detail preserves the normalized language
- `mol` and `chemdraw` line growth receives a molecular edit award
- molecular edit awards scale by `added_lines * 5`
- `kernel_free_truth_diff_result(...)` releases all owned strings and leaves the
  result empty
- Tauri sealed bridge serializes truth diff kernel results to JSON without
  Rust-owned truth diff C ABI structs or unsafe result-copy loops
- Tauri Rust bridge tests continue to cover the existing command-facing
  localized reason strings for code-language and molecular awards

## Semantic Context

The repository must retain regression coverage for:

- `kernel_build_semantic_context(...)` trims short content
- long semantic context extraction keeps the last four eligible Markdown
  headings in original order
- long semantic context extraction keeps the last three non-empty blocks in
  original order
- semantic context output preserves the existing `Headings:` and
  `Recent focus:` section shape
- semantic context output respects the `24` byte minimum joined-context
  threshold
- semantic context output respects the `2200` byte maximum return size
- `kernel_build_semantic_context(...)` rejects null output pointers
- `kernel_build_semantic_context(...)` rejects null non-empty content buffers
- `kernel_build_semantic_context(...)` accepts empty null content buffers
- `kernel_free_buffer(...)` releases semantic context output and leaves the
  buffer empty
- Tauri sealed bridge returns semantic context text without Rust-owned kernel
  buffer mirror structs or unsafe buffer-copy loops
- Tauri Rust bridge tests continue to cover the existing command-facing string
  shape for short and focused long content

## AI Host Text Limits

The repository must retain regression coverage for:

- `kernel_get_semantic_context_min_bytes(...)` returns `24`
- `kernel_get_rag_context_per_note_char_limit(...)` returns `1500`
- `kernel_get_embedding_text_char_limit(...)` returns `2000`
- all product text limit getters reject null output pointers
- Tauri Rust queries these limits through the sealed bridge instead of keeping
  duplicate product text constants

## Study Truth State

The repository must retain regression coverage for:

- `kernel_compute_truth_state_from_activity(...)` routes science, engineering,
  creation, and finance extensions through the kernel
- active seconds convert to EXP with the `60` second rule
- overall level progression uses the `100 * 1.5^(level - 1)` curve
- attribute levels use the `50` EXP per level rule capped at `99`
- empty activity starts at level `1` with next level requirement `100`
- null non-empty buffers, null note ids, and null output pointers are rejected
- Tauri Rust study DB code aggregates SQLite rows and delegates truth rules to
  the sealed kernel bridge
