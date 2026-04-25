<!-- Reason: This file records regression obligations for product compute rules as they move from Tauri Rust into the kernel. -->

# Product Compute Regression Matrix

Last updated: `2026-04-25`

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
- Tauri Rust bridge tests continue to cover the existing command-facing string
  shape for short and focused long content
