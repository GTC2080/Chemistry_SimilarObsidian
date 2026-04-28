<!-- Reason: This file freezes the host-facing product compute rules that moved from Tauri Rust into the sealed kernel. -->

# Product Compute Contract

Last updated: `2026-04-28`

## Scope

This document covers stateless product compute surfaces that are not chemistry,
crystal, symmetry, search, or vault persistence surfaces.

Current surface:

- `kernel_compute_truth_diff(prev_content, prev_size, curr_content, curr_size, file_extension, out_result)`
- `kernel_get_truth_award_reason_key(reason, out_key)`
- `kernel_free_truth_diff_result(out_result)`
- `kernel_build_semantic_context(content, content_size, out_buffer)`
- `kernel_get_semantic_context_min_bytes(out_bytes)`
- `kernel_get_rag_context_per_note_char_limit(out_chars)`
- `kernel_get_embedding_text_char_limit(out_chars)`
- `kernel_get_ai_chat_timeout_secs(out_secs)`
- `kernel_get_ai_ponder_timeout_secs(out_secs)`
- `kernel_get_ai_embedding_request_timeout_secs(out_secs)`
- `kernel_get_ai_embedding_cache_limit(out_limit)`
- `kernel_get_ai_embedding_concurrency_limit(out_limit)`
- `kernel_compute_truth_state_from_activity(activities, activity_count, out_state)`
- `kernel_compute_study_stats_window(now_epoch_secs, days_back, out_window)`
- `kernel_compute_study_streak_days(day_buckets, day_count, today_bucket, out_streak_days)`
- `kernel_compute_study_streak_days_from_timestamps(started_at_epoch_secs, timestamp_count, today_bucket, out_streak_days)`
- `kernel_build_study_heatmap_grid(days, day_count, now_epoch_secs, out_grid)`
- `kernel_free_study_heatmap_grid(out_grid)`

Current exclusions:

- vault reads or writes
- database writes
- UI state
- localized display text

## Boundary

Frozen rules:

- the surface is handle-free and must not read or write vault state
- Tauri Rust owns serde command marshalling and localized reason text
- the kernel owns award attribute routing, award amounts, reason keys,
  code-fence language detection, and molecular line-growth detection
- the kernel owns semantic context trimming, heading extraction, recent-block
  selection, and context length limits
- the kernel owns host-facing AI/product text limits used for semantic context
  gating, RAG note snippets, and embedding request input trimming
- the kernel owns host-facing AI runtime defaults for chat, ponder, embedding
  request timeout, embedding cache size, and embedding concurrency
- the kernel owns study truth attribute routing, active-seconds to EXP
  conversion, level progression, and attribute level progression
- the kernel owns study stats UTC day boundary calculation, week/daily/legacy
  heatmap window starts, current day bucket, and folder ranking limit
- the kernel owns study streak timestamp bucketing, duplicate-day handling, and
  contiguous-day counting
- the kernel owns study heatmap grid dimensions, UTC day bucketing, Monday
  alignment, date formatting, cell coordinates, and max-second calculation
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
- hosts must not hard-code semantic context gating, RAG note snippet, or
  embedding input text limits
- hosts must not hard-code AI chat, ponder, embedding timeout, cache, or
  concurrency defaults
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
- null output pointers are invalid

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
- hosts may aggregate activity rows from SQLite and add localized timestamps,
  but must call `kernel_compute_truth_state_from_activity(...)` for the rules
- null non-empty activity buffers, null note ids, and null output pointers are
  invalid

## Study Stats Window

Frozen rules:

- study stats window input is the current epoch seconds plus `days_back`
- the current day is floored to UTC midnight
- `today_bucket = floor(today_start_epoch_secs / 86400)`
- `week_start_epoch_secs = today_start_epoch_secs - 6 * 86400`
- `daily_window_start_epoch_secs = today_start_epoch_secs - (days_back - 1) * 86400`
- `heatmap_start_epoch_secs = today_start_epoch_secs - 179 * 86400`
- `folder_rank_limit = 5`
- hosts may query SQLite using these returned boundaries, but must not compute
  or hard-code them outside the kernel
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
