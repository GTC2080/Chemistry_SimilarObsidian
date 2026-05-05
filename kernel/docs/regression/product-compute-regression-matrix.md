<!-- Reason: This file records regression obligations for product compute rules as they move from Tauri Rust into the kernel. -->

# Product Compute Regression Matrix

Last updated: `2026-05-05`

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

## File Extension Derivation

The repository must retain regression coverage for:

- `kernel_derive_file_extension_from_path(...)` lower-cases final-segment
  extensions
- Windows `\` and POSIX `/` separators both delimit path segments
- dots in parent directories are ignored
- extensionless final segments return empty text
- null non-empty path buffers and null output pointers are rejected
- Tauri Rust media commands ask the sealed bridge for spectroscopy and molecular
  preview file extensions instead of deriving them with Rust path helpers
- Tauri Rust legacy embedding-cache metadata asks the sealed bridge for file
  extensions instead of deriving them with Rust path helpers
- Tauri Rust note catalog DTO mapping asks the sealed bridge for `NoteInfo`
  file extensions instead of deriving them with Rust path helpers

## Note Display Name Derivation

The repository must retain regression coverage for:

- `kernel_derive_note_display_name_from_path(...)` strips only the final path
  segment extension
- Windows `\` and POSIX `/` separators both delimit path segments
- dots in parent directories are ignored
- extensionless names and dotfiles without another dot are preserved
- null non-empty path buffers and null output pointers are rejected
- Tauri Rust note catalog DTO mapping asks the sealed bridge for `NoteInfo.name`
  instead of deriving it with Rust path helpers

## Database Grid Normalization

The repository must retain regression coverage for:

- `kernel_normalize_database_json(...)` trims string column ids and names
- non-string or empty column names normalize to `Untitled`
- `kernel_normalize_database_column_type(...)` preserves `text`, `number`,
  `select`, and `tags`
- payload column types use the same kernel allow-list, with unknown and empty
  types falling back to `text`
- missing valid columns generate `Name`, `Tags`, and `Notes` defaults
- string row ids are trimmed, non-object rows are skipped, and generated row ids
  remain kernel-owned
- row cells are emitted only for normalized column ids in column order
- missing cells normalize to an empty string and extra input cells are dropped
- valid scalar/array/object/null cell JSON shapes are preserved
- invalid JSON, null non-empty input buffers, and null output pointers are
  rejected
- Tauri Rust `normalize_database` asks the sealed bridge for complete payload
  normalization instead of retaining local column, row, or cell rules

## Paper Compile Defaults And Diagnostics

The repository must retain regression coverage for:

- `kernel_get_default_paper_template(...)` returns `standard-thesis`
- `kernel_build_paper_compile_plan_json(...)` keeps template argument selection,
  CSL/bibliography trimming, and resource-path deduplication in the kernel
- `kernel_summarize_paper_compile_log_json(...)` extracts ordered compile
  highlights and caps them at `12` lines
- `kernel_summarize_paper_compile_log_json(...)` truncates log prefixes by
  Unicode character count without splitting UTF-8 codepoints
- `kernel_summarize_paper_compile_log_json(...)` reports whether truncation
  occurred
- Tauri Rust `compiler.rs` asks the sealed bridge for the default template and
  log summary instead of hard-coding `standard-thesis`, `trim_log`, or
  `extract_latex_error`

## PubChem Compound Info

The repository must retain regression coverage for:

- `kernel_normalize_pubchem_query(...)` trims query text and rejects blank input
- `kernel_build_pubchem_compound_info_json(...)` returns `notFound` for empty
  property lists
- `kernel_build_pubchem_compound_info_json(...)` returns `ambiguous` for
  multiple property rows
- valid PubChem rows trim formulas, preserve positive finite molecular weights,
  and include only positive finite density values
- invalid formula or molecular-weight data is classified as `notFound`
- Tauri Rust `chem_api.rs` keeps only HTTP, response DTO, and localized error
  mapping responsibilities, with compound property classification delegated to
  the sealed bridge

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
- `kernel_get_ai_rag_top_note_limit(...)` returns `5`
- all product text limit getters reject null output pointers
- Tauri Rust queries semantic text limits through the sealed bridge and relies
  on `kernel_normalize_ai_embedding_text(...)` /
  `kernel_build_ai_rag_context(...)` for embedding and RAG text limits instead
  of keeping duplicate product text constants
- Tauri Rust AI code queries chat, ponder, embedding timeout, cache,
  concurrency, and RAG top-note defaults through the sealed bridge instead of
  keeping duplicate runtime constants

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
- `kernel_is_ai_embedding_text_indexable(...)` returns true for non-empty
  normalized embedding text
- `kernel_is_ai_embedding_text_indexable(...)` returns false for empty or
  all-whitespace text instead of forcing hosts to catch normalization errors
- `kernel_is_ai_embedding_text_indexable(...)` applies the same 2000 Unicode
  character truncation before deciding whether text is indexable
- `kernel_is_ai_embedding_text_indexable(...)` rejects null non-empty input
  buffers and null output pointers
- `kernel_compute_ai_embedding_cache_key(...)` returns a stable 16-hex key for
  `(base_url, model, normalized text)`
- `kernel_compute_ai_embedding_cache_key(...)` changes when normalized text
  changes
- `kernel_compute_ai_embedding_cache_key(...)` rejects null non-empty input
  buffers and null output pointers
- `kernel_should_refresh_ai_embedding_note(...)` returns true when the
  compatibility cache has no existing timestamp
- `kernel_should_refresh_ai_embedding_note(...)` returns true when the note
  timestamp is newer than the compatibility cache timestamp
- `kernel_should_refresh_ai_embedding_note(...)` returns false for equal or
  newer compatibility cache timestamps
- `kernel_should_refresh_ai_embedding_note(...)` rejects null output pointers
- `kernel_serialize_ai_embedding_blob(...)` encodes f32 vectors as stable
  little-endian BLOB bytes
- `kernel_parse_ai_embedding_blob(...)` round-trips the same stable
  little-endian f32 BLOB format
- `kernel_parse_ai_embedding_blob(...)` rejects BLOB byte counts that are not
  divisible by four
- Tauri Rust AI code delegates embedding input normalization to the sealed
  bridge instead of calling `chars().take(...)` locally
- Tauri Rust indexing preflight delegates embedding text indexability to the
  sealed bridge instead of calling `trim().is_empty()` locally
- Tauri Rust AI code delegates embedding cache-key derivation to the sealed
  bridge instead of using Rust `DefaultHasher`
- Tauri Rust vault indexing delegates embedding note refresh decisions to the
  sealed bridge instead of comparing `note.updated_at` and cached timestamps
  locally
- kernel embedding storage continues to use the kernel-owned f32 vector BLOB
  codec instead of host code using `from_raw_parts`, `chunks_exact`, or local
  endian conversion
- Tauri Rust embedding storage and semantic top-k retrieval are covered by
  `regression/ai-embedding-cache-regression-matrix.md`

## AI Prompt Shape

The repository must retain regression coverage for:

- `kernel_build_ai_rag_context(...)` preserves note headers, 1-based note
  numbering, note names, note content, and blank-line separators
- `kernel_build_ai_rag_context(...)` skips empty or all-whitespace note content
  and keeps emitted note numbering contiguous
- `kernel_build_ai_rag_context(...)` truncates note content at the kernel-owned
  `1500` Unicode character limit rather than at raw bytes
- `kernel_build_ai_rag_context(...)` returns empty text for an empty note list
- `kernel_build_ai_rag_context(...)` rejects null non-empty note buffers and
  null output pointers
- `kernel_build_ai_rag_context_from_note_paths(...)` derives display names from
  final path segments, strips final extensions, and preserves extensionless
  names
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
- Tauri Rust AI code delegates RAG note context formatting, note display-name
  derivation, and truncation to the sealed bridge instead of stitching note
  headers or deriving display names locally

## Study Truth State

The repository must retain regression coverage for:

- `kernel_compute_truth_state_from_activity(...)` routes science, engineering,
  creation, and finance extensions through the kernel
- active seconds convert to EXP with the `60` second rule
- overall level progression uses the `100 * 1.5^(level - 1)` curve
- attribute levels use the `50` EXP per level rule capped at `99`
- empty activity starts at level `1` with next level requirement `100`
- null non-empty buffers, null note ids, and null output pointers are rejected
- handle-bound `kernel_query_study_truth_state_json(...)` reads
  kernel-owned session rows and returns the host-facing TruthState JSON
- Tauri Rust `cmd_study.rs` delegates truth queries to the sealed kernel bridge
  without retaining a SQLite connection, schema, or aggregation SQL

## Study Session Storage

The repository must retain regression coverage for:

- `kernel_start_study_session(...)` inserts a row and returns a generated
  session id
- `kernel_tick_study_session(...)` and `kernel_end_study_session(...)` add
  active seconds to the existing row
- stats, truth state, and heatmap queries observe the accumulated session rows
- invalid handles, null note/folder pointers, null output pointers, and missing
  session rows are rejected
- Tauri Rust study commands call the sealed bridge storage/query APIs instead
  of creating `src/db` or using `rusqlite`

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
- handle-bound `kernel_query_study_stats_json(...)` returns today/week/daily
  summaries and folder ranking from kernel-owned session rows
- Tauri Rust study commands do not hard-code study stats windows, folder
  ranking limits, or aggregation SQL

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
- handle-bound study stats queries derive streaks from kernel-owned session
  rows through the same kernel bucketing and continuity rules

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
- handle-bound `kernel_query_study_heatmap_grid_json(...)` reads
  kernel-owned daily rows and returns the host-facing heatmap JSON
- Tauri Rust study commands do not read SQLite daily rows or rebuild heatmap
  grid calendar/layout rules
