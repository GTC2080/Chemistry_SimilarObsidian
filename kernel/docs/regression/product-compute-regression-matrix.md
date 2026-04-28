<!-- Reason: This file records regression obligations for product compute rules as they move from Tauri Rust into the kernel. -->

# Product Compute Regression Matrix

Last updated: `2026-04-28`

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
- `kernel_get_truth_award_reason_key(...)` returns `textDelta`,
  `codeLanguage`, and `molecularEdit`
- `kernel_get_truth_award_reason_key(...)` rejects null outputs and unknown
  enum values
- `kernel_free_truth_diff_result(...)` releases all owned strings and leaves the
  result empty
- Tauri sealed bridge serializes truth diff kernel results to JSON without
  Rust-owned truth diff C ABI structs, mirrored reason enum integers, or unsafe
  result-copy loops
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

## AI Host Limits

The repository must retain regression coverage for:

- `kernel_get_semantic_context_min_bytes(...)` returns `24`
- `kernel_get_rag_context_per_note_char_limit(...)` returns `1500`
- `kernel_get_embedding_text_char_limit(...)` returns `2000`
- `kernel_get_ai_chat_timeout_secs(...)` returns `120`
- `kernel_get_ai_ponder_timeout_secs(...)` returns `60`
- `kernel_get_ai_embedding_request_timeout_secs(...)` returns `30`
- `kernel_get_ai_embedding_cache_limit(...)` returns `64`
- `kernel_get_ai_embedding_concurrency_limit(...)` returns `4`
- all product text limit getters reject null output pointers
- Tauri Rust queries semantic text limits through the sealed bridge and relies
  on `kernel_normalize_ai_embedding_text(...)` /
  `kernel_build_ai_rag_context(...)` for embedding and RAG text limits instead
  of keeping duplicate product text constants
- Tauri Rust AI code queries chat, ponder, embedding timeout, cache, and
  concurrency defaults through the sealed bridge instead of keeping duplicate
  runtime constants

## AI Embedding Input

The repository must retain regression coverage for:

- `kernel_normalize_ai_embedding_text(...)` preserves non-empty caller text
  shape after truncation
- `kernel_normalize_ai_embedding_text(...)` truncates input at the kernel-owned
  `2000` Unicode character limit rather than at raw bytes
- `kernel_normalize_ai_embedding_text(...)` rejects empty or all-whitespace
  input after truncation
- `kernel_normalize_ai_embedding_text(...)` rejects null non-empty input
  buffers and null output pointers
- `kernel_compute_ai_embedding_cache_key(...)` returns a stable 16-hex key for
  `(base_url, model, normalized text)`
- `kernel_compute_ai_embedding_cache_key(...)` changes when normalized text
  changes
- `kernel_compute_ai_embedding_cache_key(...)` rejects null non-empty input
  buffers and null output pointers
- Tauri Rust AI code delegates embedding input normalization to the sealed
  bridge instead of calling `chars().take(...)` locally
- Tauri Rust AI code delegates embedding cache-key derivation to the sealed
  bridge instead of using Rust `DefaultHasher`

## AI Prompt Shape

The repository must retain regression coverage for:

- `kernel_build_ai_rag_context(...)` preserves note headers, 1-based note
  numbering, note names, note content, and blank-line separators
- `kernel_build_ai_rag_context(...)` truncates note content at the kernel-owned
  `1500` Unicode character limit rather than at raw bytes
- `kernel_build_ai_rag_context(...)` returns empty text for an empty note list
- `kernel_build_ai_rag_context(...)` rejects null non-empty note buffers and
  null output pointers
- `kernel_build_ai_rag_system_content(...)` preserves the private
  knowledge-base system prompt plus related-note context header
- `kernel_get_ai_ponder_system_prompt(...)` preserves the strict JSON-array and
  no-Markdown instructions
- `kernel_build_ai_ponder_user_prompt(...)` preserves the topic/context lines
  and the 3-to-5-node instruction
- `kernel_get_ai_ponder_temperature(...)` returns `0.7`
- prompt builders reject null non-empty buffers and null output pointers
- Tauri Rust AI code queries prompt shapes and Ponder temperature through the
  sealed bridge instead of keeping duplicate prompt strings or temperature
  constants
- Tauri Rust AI code delegates RAG note context formatting and truncation to
  the sealed bridge instead of stitching note headers locally

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

## Study Stats Window

The repository must retain regression coverage for:

- `kernel_compute_study_stats_window(...)` floors current epoch seconds to UTC
  midnight
- the current day bucket is returned by the kernel
- week stats keep the legacy six-day lookback from today
- daily summary/detail windows include exactly `days_back` days
- legacy stats heatmap reads keep the `179` day lookback from today
- folder ranking limit remains `5` and comes from the kernel
- non-positive `days_back` and null output pointers are rejected
- Tauri Rust study stats code queries SQLite with kernel-returned boundaries
  and does not hard-code study stats windows or folder ranking limits

## Study Streak

The repository must retain regression coverage for:

- `kernel_compute_study_streak_days(...)` counts contiguous active days through
  the current day bucket
- `kernel_compute_study_streak_days_from_timestamps(...)` owns timestamp to day
  bucket conversion before contiguous-day counting
- missing current-day activity returns `0`
- duplicate input day buckets are counted once
- duplicate timestamp bucket days are counted once
- negative epoch seconds use floor-division day bucketing
- input order does not affect the result
- empty bucket input returns `0`
- null non-empty bucket/timestamp buffers and null output pointers are rejected
- Tauri Rust study stats code reads SQLite timestamps and delegates day
  bucketing plus streak continuity rules to the sealed kernel bridge

## Study Heatmap Grid

The repository must retain regression coverage for:

- `kernel_build_study_heatmap_grid(...)` returns `26 * 7` cells
- the generated grid starts on Monday and ends on the current UTC day
- duplicate daily rows are summed before assigning cell seconds
- stale out-of-window daily rows do not affect cells or `max_secs`
- returned cell coordinates preserve `col = week` and `row = day-of-week`
- `kernel_free_study_heatmap_grid(...)` releases owned date strings and resets
  the grid
- null non-empty buffers, null date pointers, and null output pointers are
  rejected
- Tauri Rust study stats code reads SQLite daily rows and delegates heatmap
  grid calendar/layout rules to the sealed kernel bridge
