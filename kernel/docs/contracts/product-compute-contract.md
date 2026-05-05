<!-- Reason: This file freezes the host-facing product compute rules that moved from Tauri Rust into the sealed kernel. -->

# Product Compute Contract

Last updated: `2026-05-05`

## Scope

This document covers product compute surfaces and the study/session product
storage/query surface that are not chemistry, crystal, symmetry, search, or
general vault persistence surfaces.

Current surface:

- `kernel_compute_truth_diff(prev_content, prev_size, curr_content, curr_size, file_extension, out_result)`
- `kernel_get_truth_award_reason_key(reason, out_key)`
- `kernel_free_truth_diff_result(out_result)`
- `kernel_build_semantic_context(content, content_size, out_buffer)`
- `kernel_derive_file_extension_from_path(path, path_size, out_buffer)`
- `kernel_derive_note_display_name_from_path(path, path_size, out_buffer)`
- `kernel_normalize_database_column_type(column_type, column_type_size, out_buffer)`
- `kernel_normalize_database_json(json, json_size, out_buffer)`
- `kernel_get_semantic_context_min_bytes(out_bytes)`
- `kernel_get_rag_context_per_note_char_limit(out_chars)`
- `kernel_get_embedding_text_char_limit(out_chars)`
- `kernel_get_ai_chat_timeout_secs(out_secs)`
- `kernel_get_ai_ponder_timeout_secs(out_secs)`
- `kernel_get_ai_embedding_request_timeout_secs(out_secs)`
- `kernel_get_ai_embedding_cache_limit(out_limit)`
- `kernel_get_ai_embedding_concurrency_limit(out_limit)`
- `kernel_get_ai_rag_top_note_limit(out_limit)`
- `kernel_normalize_ai_embedding_text(text, text_size, out_buffer)`
- `kernel_is_ai_embedding_text_indexable(text, text_size, out_is_indexable)`
- `kernel_should_refresh_ai_embedding_note(note_updated_at, has_existing_updated_at, existing_updated_at, out_should_refresh)`
- `kernel_compute_ai_embedding_cache_key(base_url, base_url_size, model, model_size, text, text_size, out_buffer)`
- `kernel_serialize_ai_embedding_blob(values, value_count, out_buffer)`
- `kernel_parse_ai_embedding_blob(blob, blob_size, out_values)`
- handle-bound AI embedding metadata/vector storage and top-k retrieval are
  documented in `contracts/ai-embedding-cache-contract.md`
- `kernel_build_ai_rag_context(note_names, note_name_sizes, note_contents, note_content_sizes, note_count, out_buffer)`
- `kernel_build_ai_rag_context_from_note_paths(note_paths, note_path_sizes, note_contents, note_content_sizes, note_count, out_buffer)`
- `kernel_build_ai_rag_system_content(context, context_size, out_buffer)`
- `kernel_get_ai_ponder_system_prompt(out_buffer)`
- `kernel_build_ai_ponder_user_prompt(topic, topic_size, context, context_size, out_buffer)`
- `kernel_get_ai_ponder_temperature(out_temperature)`
- `kernel_compute_truth_state_from_activity(activities, activity_count, out_state)`
- `kernel_compute_study_stats_window(now_epoch_secs, days_back, out_window)`
- `kernel_compute_study_streak_days(day_buckets, day_count, today_bucket, out_streak_days)`
- `kernel_compute_study_streak_days_from_timestamps(started_at_epoch_secs, timestamp_count, today_bucket, out_streak_days)`
- `kernel_build_study_heatmap_grid(days, day_count, now_epoch_secs, out_grid)`
- `kernel_free_study_heatmap_grid(out_grid)`
- `kernel_start_study_session(handle, note_id, folder, out_session_id)`
- `kernel_tick_study_session(handle, session_id, active_secs)`
- `kernel_end_study_session(handle, session_id, active_secs)`
- `kernel_query_study_stats_json(handle, now_epoch_secs, days_back, out_buffer)`
- `kernel_query_study_truth_state_json(handle, now_epoch_millis, out_buffer)`
- `kernel_query_study_heatmap_grid_json(handle, now_epoch_secs, out_buffer)`

Current exclusions:

- general vault reads or writes outside the documented study/session storage
- host-owned database writes outside the kernel
- UI state
- localized display text

## Boundary

Frozen rules:

- stateless compute surfaces are handle-free and must not read or write vault
  state
- handle-bound study/session surfaces may read and write only the
  kernel-owned `study_sessions` storage table for the active vault handle
- Tauri Rust owns serde command marshalling and localized reason text
- the kernel owns award attribute routing, award amounts, reason keys,
  code-fence language detection, and molecular line-growth detection
- the kernel owns semantic context trimming, heading extraction, recent-block
  selection, and context length limits
- the kernel owns host-facing file extension derivation from path strings used by
  media/product compute commands
- the kernel owns host-facing note display-name derivation from path strings used
  by note catalog DTOs and AI RAG context
- the kernel owns host-facing database grid payload normalization, including
  column type normalization
- the kernel owns host-facing AI/product text limits used for semantic context
  gating, RAG note snippets, and embedding request input trimming
- the kernel owns host-facing AI runtime defaults for chat, ponder, embedding
  request timeout, embedding cache size, embedding concurrency, and RAG top-note
  count
- the kernel owns AI embedding input normalization, Unicode character
  truncation, empty-after-truncation rejection, indexability preflight, and
  cache-key derivation
- the kernel owns the AI embedding note refresh decision used by embedding
  indexing workflows
- the kernel owns the stable AI embedding vector BLOB codec used by embedding
  storage
- the kernel owns AI RAG note context formatting, blank-note skipping, note
  display-name derivation from note paths, note numbering, note separators, and
  per-note Unicode character truncation
- the kernel owns AI RAG system prompt composition, Ponder system prompt,
  Ponder user prompt shape, and Ponder temperature
- the kernel owns study truth attribute routing, active-seconds to EXP
  conversion, level progression, and attribute level progression
- the kernel owns study stats UTC day boundary calculation, week/daily/legacy
  heatmap window starts, current day bucket, and folder ranking limit
- the kernel owns study streak timestamp bucketing, duplicate-day handling, and
  contiguous-day counting
- the kernel owns study heatmap grid dimensions, UTC day bucketing, Monday
  alignment, date formatting, cell coordinates, and max-second calculation
- the kernel owns study session persistence, active-second increments, stats
  aggregation rows, truth-state activity aggregation, and heatmap daily rows
- returned awards and strings are kernel-owned until released with
  `kernel_free_truth_diff_result(...)`
- returned semantic context bytes are kernel-owned until released with
  `kernel_free_buffer(...)`
- host sealed bridges may serialize truth awards and semantic context into
  host-owned JSON/text before crossing into higher-level runtimes
- `reason` is a typed enum; hosts must request a stable reason key from
  `kernel_get_truth_award_reason_key(...)` before mapping it to localized text
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
- hosts must not hard-code truth award enum integer values
- hosts must not reimplement semantic context extraction rules
- hosts must not reimplement media/product file extension derivation with local
  path libraries or string slicing
- hosts must not reimplement note display-name derivation from rel paths with
  local path libraries or string slicing
- hosts must not keep local database column type allow-lists, default-column
  rules, row/cell fill rules, or extra-cell pruning rules for database grid
  normalization
- hosts must not hard-code semantic context gating, RAG note snippet, or
  embedding input text limits
- hosts must not reimplement embedding input truncation, empty-text checks, or
  embedding indexability / cache-key derivation
- hosts must not locally compare note timestamps to decide whether embedding
  cache metadata should refresh
- hosts must not reinterpret `float` memory, hand-roll little-endian f32 BLOB
  serialization, or locally decide malformed embedding BLOB validity
- hosts must not create, query, mutate, or rank a host-owned embedding SQL/cache
  store once the kernel `ai_embedding_cache` surface is available
- hosts must not hard-code AI chat, ponder, embedding timeout, cache,
  concurrency, or RAG top-note defaults
- hosts must not hard-code AI RAG note context headers, blank-note skipping,
  note display-name derivation, note numbering, separators, or per-note
  truncation behavior
- hosts must not hard-code AI RAG/Ponder prompt text, Ponder user prompt shape,
  or Ponder temperature
- hosts must not reimplement study truth EXP curves or note-extension to
  attribute routing
- hosts must not reimplement study stats window or folder ranking limit rules
- hosts must not reimplement study streak timestamp bucketing or contiguous-day
  rules
- hosts must not reimplement study heatmap grid calendar or layout rules
- Rust hosts must not retain product compute C ABI mirror structs or unsafe
  result-copy loops for truth diff awards or semantic context buffers
- hosts may map kernel-provided truth award reason keys to localized reason
  strings
- frontends continue to consume host commands rather than kernel ABI directly

## File Extension Derivation

Frozen rules:

- `kernel_derive_file_extension_from_path(...)` reads only the final path segment
  after `/` or `\`
- parent directory dots are ignored
- the extension is the text after the final `.` in the final segment
- returned extensions are lower-case ASCII
- extensionless names, dotfiles without another dot, trailing dots, and trailing
  separators return empty text
- null non-empty path buffers and null output buffers are invalid

## Note Display Name Derivation

Frozen rules:

- `kernel_derive_note_display_name_from_path(...)` reads only the final path
  segment after `/` or `\`
- parent directory dots are ignored
- names with a final extension return the text before the final `.` in the final
  segment
- extensionless names are preserved
- dotfiles without another dot are preserved
- matching is byte-preserving; display names are not lower-cased
- null non-empty path buffers and null output buffers are invalid

## Database Grid Rules

Frozen rules:

- `kernel_normalize_database_json(...)` accepts a JSON value and returns a
  normalized database payload JSON object with `columns` and `rows`
- non-object inputs normalize as an empty object payload
- column entries are read only from an input object `columns` array
- non-object columns and columns missing a `name` field are skipped
- column `id` values are trimmed when they are strings; missing, non-string, or
  empty ids receive generated `col...` ids
- column `name` values are trimmed when they are strings; non-string and empty
  names normalize to `Untitled`
- `kernel_normalize_database_column_type(...)` preserves `text`, `number`,
  `select`, and `tags`
- unknown column types return `text`
- empty column type input returns `text`
- matching is exact and case-sensitive
- `kernel_normalize_database_json(...)` applies the same column type rules to
  every payload column
- if no valid columns remain, default columns are generated as `Name` / `text`,
  `Tags` / `tags`, and `Notes` / `text`
- row entries are read only from an input object `rows` array
- non-object rows are skipped
- row `id` values are trimmed when they are strings; missing, non-string, or
  empty ids receive generated `row...` ids
- cells are emitted only for normalized column ids, in column order
- missing cells normalize to an empty string
- extra input cells outside normalized column ids are dropped
- cell JSON values preserve their input JSON shape
- invalid JSON, null non-empty input buffers, and null output buffers are
  invalid

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

## AI Host Limits

Frozen rules:

- `kernel_get_rag_context_per_note_char_limit(...) = 1500`
- `kernel_get_embedding_text_char_limit(...) = 2000`
- `kernel_get_ai_chat_timeout_secs(...) = 120`
- `kernel_get_ai_ponder_timeout_secs(...) = 60`
- `kernel_get_ai_embedding_request_timeout_secs(...) = 30`
- `kernel_get_ai_embedding_cache_limit(...) = 64`
- `kernel_get_ai_embedding_concurrency_limit(...) = 4`
- `kernel_get_ai_rag_top_note_limit(...) = 5`
- null output pointers are invalid

## AI Embedding Input

Frozen rules:

- `kernel_normalize_ai_embedding_text(...)` truncates input to `2000` Unicode
  characters
- `kernel_normalize_ai_embedding_text(...)` preserves the caller-provided text
  shape after truncation; it does not trim returned text
- `kernel_normalize_ai_embedding_text(...)` rejects input that is empty or only
  Unicode whitespace after truncation
- `kernel_is_ai_embedding_text_indexable(...)` returns `0` instead of failing
  for empty or all-whitespace-after-truncation text, so hosts can prefilter
  indexing work without duplicating whitespace or truncation rules
- `kernel_compute_ai_embedding_cache_key(...)` returns a stable
  16-hex-character key for `(base_url, model, normalized text)`
- `kernel_compute_ai_embedding_cache_key(...)` field-separates base URL, model,
  and normalized text before hashing so adjacent field concatenation cannot
  collide
- `kernel_should_refresh_ai_embedding_note(...)` returns true when no existing
  timestamp is present
- `kernel_should_refresh_ai_embedding_note(...)` returns true only when
  `note_updated_at > existing_updated_at` when an existing timestamp is present
- `kernel_should_refresh_ai_embedding_note(...)` returns false for equal or
  newer compatibility-cache timestamps
- `kernel_serialize_ai_embedding_blob(...)` encodes each embedding value as one
  stable little-endian IEEE-754 32-bit float
- `kernel_parse_ai_embedding_blob(...)` decodes the same little-endian f32 BLOB
  format and rejects byte counts that are not divisible by four
- null non-empty input buffers and null output pointers are invalid

## AI Prompt Shape

Frozen rules:

- `kernel_build_ai_rag_context(...)` returns one block per note shaped as
  `--- 笔记 {1-based index} 《{note name}》 ---`, followed by a newline, the
  note content truncated to `1500` Unicode characters, and a blank-line
  separator
- note entries whose content is empty or only Unicode whitespace are skipped
  before formatting
- skipped blank notes do not consume note numbers; emitted block numbering
  remains contiguous and 1-based
- `kernel_build_ai_rag_context(...)` returns empty text for an empty note list
- `kernel_build_ai_rag_context_from_note_paths(...)` derives the note display
  name from the final path segment, strips the final extension, and preserves
  extensionless names
- `kernel_build_ai_rag_system_content(...)` prepends the private knowledge-base
  RAG system prompt, a blank line, the related-note context header, another
  blank line, then the caller-provided context
- `kernel_get_ai_ponder_system_prompt(...)` returns the strict JSON-array
  Ponder system prompt
- `kernel_build_ai_ponder_user_prompt(...)` returns exactly the topic line,
  context line, and 3-to-5-node instruction used by the Ponder workflow
- `kernel_get_ai_ponder_temperature(...) = 0.7`
- null non-empty note arrays, null non-empty prompt buffers, and null output
  pointers are invalid

## Study Truth State

Frozen rules:

- study truth input is a handle-free list of `(note_id, active_secs)` activity
  rows
- note extension routing matches truth diff extension routing:
  - `jdx`, `csv` -> `science`
  - engineering source extensions -> `engineering`
  - `mol`, `chemdraw` -> `creation`
  - `dashboard`, `base` -> `finance`
  - everything else -> `creation`
- `1 EXP = floor(active_secs / 60)`
- total level starts at `1`
- next-level EXP requirement is `floor(100 * 1.5^(level - 1))`
- attribute level is `min(99, 1 + attribute_exp / 50)`
- handle-free callers may pass explicit activity rows to
  `kernel_compute_truth_state_from_activity(...)`
- handle-bound hosts must use `kernel_query_study_truth_state_json(...)` so the
  kernel reads activity rows from its own storage and appends the requested
  settlement timestamp
- null non-empty activity buffers, null note ids, and null output pointers are
  invalid

## Study Session Storage

Frozen rules:

- `kernel_start_study_session(...)` inserts into the kernel-owned
  `study_sessions` table and returns the generated session id
- `kernel_tick_study_session(...)` and `kernel_end_study_session(...)` add
  non-negative active seconds to the existing session row
- note ids and folders are host-provided UTF-8 text and must not be null
- invalid handles, null output pointers, and missing session rows are errors
- Tauri Rust must not create a parallel SQLite connection, schema, or
  host-side study/session table

## Study Stats Window

Frozen rules:

- study stats window input is the current epoch seconds plus `days_back`
- the current day is floored to UTC midnight
- `today_bucket = floor(today_start_epoch_secs / 86400)`
- `week_start_epoch_secs = today_start_epoch_secs - 6 * 86400`
- `daily_window_start_epoch_secs = today_start_epoch_secs - (days_back - 1) * 86400`
- `heatmap_start_epoch_secs = today_start_epoch_secs - 179 * 86400`
- `folder_rank_limit = 5`
- handle-free callers may use these returned boundaries for compatibility
  tests, but handle-bound Tauri commands must use
  `kernel_query_study_stats_json(...)`
- Tauri Rust must not compute or hard-code study stats windows, folder ranking
  limits, or aggregation SQL outside the kernel
- non-positive `days_back` and null output pointers are invalid

## Study Streak

Frozen rules:

- study streak input is a handle-free list of day buckets plus the current day
  bucket
- hosts may alternatively pass raw `started_at` epoch seconds through
  `kernel_compute_study_streak_days_from_timestamps(...)`
- timestamp inputs are bucketed with floor division by `86400`
- input order is not significant
- duplicate day buckets are counted once
- streak starts at the current day bucket and walks backward one day at a time
- missing current-day activity returns `0`
- null non-empty bucket/timestamp buffers and null output pointers are invalid

## Study Heatmap Grid

Frozen rules:

- heatmap grid input is a handle-free list of `(date, active_secs)` daily rows
  plus the current epoch seconds
- grid shape is `26` weeks by `7` days
- the current day is floored to UTC midnight
- the first cell is aligned back to Monday
- dates are formatted as `YYYY-MM-DD`
- duplicate input dates are summed
- cells are emitted in column-major week order with `col = week` and
  `row = day-of-week`
- `max_secs` is the maximum cell seconds in the returned grid
- returned cell date strings are kernel-owned until released with
  `kernel_free_study_heatmap_grid(...)`
- null non-empty day buffers, null date pointers, and null output pointers are
  invalid
