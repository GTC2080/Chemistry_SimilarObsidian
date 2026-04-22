<!-- Reason: This file freezes the Track 1 host-facing search contract so snippet, pagination, filters, and ranking land on one stable ABI surface. -->

# Search Query Contract

Last updated: `2026-04-21`

## Scope

This document freezes the Track 1 host-facing behavior for:

- `kernel_query_search(...)`
- `kernel_free_search_page(...)`

It is the formal search-expansion contract for hosts.
It does not replace the legacy query surface document for tags, backlinks, attachments, or the deprecated Phase 1 note-search entry points.

## Legacy Boundary

- `kernel_search_notes(...)` and `kernel_search_notes_limited(...)` remain `deprecated-but-supported`
- Track 1 does not add `snippet / pagination / filters / ranking` behavior to the legacy note-search APIs
- Track 1 completes on the new search ABI without adding new behavior to the legacy note-search ABI
- all new search behavior lands only on `kernel_query_search(...)`

## Batch 4 Request Contract

`kernel_query_search(...)` accepts `kernel_search_query`.

Batch 4 freezes these request rules:

- `handle` must be non-null
- `request` must be non-null
- `out_page` must be non-null
- `query` must be non-null, non-empty, and not whitespace-only
- `limit` must be greater than zero
- `limit` must be less than or equal to the frozen page maximum
- `offset` is zero or greater
- `kind` must be one of:
  - `KERNEL_SEARCH_KIND_NOTE`
  - `KERNEL_SEARCH_KIND_ATTACHMENT`
  - `KERNEL_SEARCH_KIND_ALL`
- `tag_filter` may be omitted for all kinds
- `tag_filter` is allowed only for `NOTE` and `ALL`
- `tag_filter` on `ATTACHMENT` is invalid
- `path_prefix` may be omitted
- non-empty `path_prefix` must be a valid relative path
- non-empty `path_prefix` is normalized to the kernel's canonical relative-path form before evaluation
- `path_prefix` applies to the result object's own `rel_path`
- `include_deleted` must be zero
- `sort_mode` may be one of:
  - `KERNEL_SEARCH_SORT_REL_PATH_ASC`
  - `KERNEL_SEARCH_SORT_RANK_V1`
- `ATTACHMENT + RANK_V1` is invalid
- `NOTE + RANK_V1` is valid
- `ALL + RANK_V1` is valid

Any request that violates one of these Batch 4 rules returns `KERNEL_ERROR_INVALID_ARGUMENT`.

## Batch 4 Result Page Contract

`kernel_query_search(...)` returns `kernel_search_page`.

Shared page rules:

- result order is deterministic
- `total_hits` is the exact hit count for the same query snapshot that produced the returned page
- `total_hits` is never an estimate
- `has_more` is exact for the returned page
- if `offset + count < total_hits`, `has_more=true`
- if `offset + count >= total_hits`, `has_more=false`
- if `offset >= total_hits`, the returned page is empty while `total_hits` remains exact
- `kernel_free_search_page(...)` is idempotent and leaves the page empty

`REL_PATH_ASC` page rules:

- `NOTE` result order is `rel_path` ascending
- `ATTACHMENT` result order is `rel_path` ascending
- `ALL` result order is:
  - notes first
  - attachments second
  - each group ordered by `rel_path` ascending

`RANK_V1` page rules:

- `NOTE` result order is:
  - title-hit boosted notes first
  - then exact single-token tag-boosted notes
  - then FTS score
  - then `rel_path` ascending
- `ALL` result order is:
  - note branch ordered by the `NOTE + RANK_V1` rules above
  - attachment branch appended after the ranked note branch
  - attachment branch ordered by `rel_path` ascending

## Batch 4 Filter Contract

The enabled Track 1 filters are:

- `kind`
- `tag_filter`
- `path_prefix`

Fixed filter semantics:

- `NOTE`
  - searches indexed note title/body
  - `tag_filter` narrows note hits to notes whose stored parser-derived tag exactly matches the supplied tag token
  - `path_prefix` narrows note hits by note `rel_path`
- `ATTACHMENT`
  - searches attachment relative paths only
  - does not search attachment contents
  - `path_prefix` narrows attachment hits by attachment `rel_path`
- `ALL`
  - returns the union of the note branch and the attachment branch
  - `tag_filter` applies to the note branch directly
  - when `tag_filter` is set on `ALL`, attachment hits are limited to attachments referenced by notes that match the supplied tag
  - `path_prefix` applies independently to each returned object's own `rel_path`

Batch 4 keeps these fields disabled:

- `include_deleted != 0`

## Batch 4 Ranking Contract

Ranking v1 is intentionally narrow and explainable.

Fixed Ranking v1 rules:

- ranking is defined only for note hits
- ranking does not add new recall
- ranking only reorders already matched note hits
- title-hit boost is based on the note title containing all query tokens
- tag exact hit boost is based on the note having a stored parser-derived tag that exactly matches the single literal query token
- tag exact hit boost applies only to single-token queries
- Ranking v1 uses the same bare tag token semantics as `kernel_query_tag_notes(...)`
- `rel_path ASC` is the stable tie-break

## Batch 4 Hit Contract

Each `kernel_search_page_hit` exposes:

- `rel_path`
- `title`
- `snippet`
- `match_flags`
- `snippet_status`
- `result_kind`
- `result_flags`
- `score`

Note-hit rules under `REL_PATH_ASC`:

- `result_kind = KERNEL_SEARCH_RESULT_NOTE`
- `result_flags = KERNEL_SEARCH_RESULT_FLAG_NONE`
- `score = 0.0`
- `match_flags` keeps the same `TITLE / BODY` semantics as the legacy note-search ABI

Note-hit rules under `RANK_V1`:

- `result_kind = KERNEL_SEARCH_RESULT_NOTE`
- `result_flags = KERNEL_SEARCH_RESULT_FLAG_NONE`
- `score` is the FTS-derived ranking component for that note hit
- higher `score` means the note has a better FTS component
- returned ordering is authoritative; title/tag boosts may change ordering between notes whose `score` values alone would not
- `score` is only meaningful within the same query result set

Attachment-hit rules:

- `result_kind = KERNEL_SEARCH_RESULT_ATTACHMENT`
- `title` is the attachment basename derived from `rel_path`
- `snippet` is empty
- `snippet_status = KERNEL_SEARCH_SNIPPET_NONE`
- `match_flags = KERNEL_SEARCH_MATCH_PATH`
- `result_flags = KERNEL_SEARCH_RESULT_FLAG_ATTACHMENT_MISSING` only when the current attachment state is missing
- `score = 0.0`

## Batch 1 Snippet Contract

Snippet behavior remains fixed as:

- note-body only
- single segment
- plain text
- fixed maximum length
- whitespace collapsed
- no rich-text rendering
- no highlight markup
- no attachment snippet

Additional fixed rules:

- the duplicated leading heading line is excluded when that heading produced the title
- title hits do not echo the title into `snippet`
- `snippet` is empty when `snippet_status=KERNEL_SEARCH_SNIPPET_TITLE_ONLY`
- `snippet` is empty when `snippet_status=KERNEL_SEARCH_SNIPPET_UNAVAILABLE`
- `snippet_status=KERNEL_SEARCH_SNIPPET_BODY_EXTRACTED` means the snippet came from the indexed note body

## Frozen Search Constants

- `search_contract_revision=track1_batch4_ranking_v1`
- `search_backend=sqlite_fts5`
- `search_snippet_mode=body_single_segment_plaintext_fixed_length`
- `search_snippet_max_bytes=160`
- `search_pagination_mode=offset_limit_exact_total_v1`
- `search_filters_mode=kind_tag_path_prefix_v1`
- `search_ranking_mode=fts_title_tag_v1`
- `search_supported_kinds=note,attachment,all`
- `search_supported_filters=kind,tag,path_prefix`
- `search_ranking_supported_kinds=note,all_note_branch`
- `search_ranking_tie_break=rel_path_asc`
- `search_all_kind_order=notes_then_attachments_rel_path_asc`
- `search_page_max_limit=128`
- `search_total_hits_supported=true`
- `search_include_deleted_supported=false`
- `search_attachment_path_only=true`
- `search_title_hit_boost_enabled=true`
- `search_tag_exact_boost_enabled=true`
- `search_tag_exact_boost_single_token_only=true`

## Error Surface

Expected public error classes for Batch 4:

- `KERNEL_OK`
- `KERNEL_ERROR_INVALID_ARGUMENT`
- `KERNEL_ERROR_IO`
- `KERNEL_ERROR_INTERNAL`

Query misses are not errors.
They return `KERNEL_OK` with:

- `count == 0`
- `total_hits == 0`
- `has_more == 0`

## Deferred Track 1 Surface

These fields already exist on the new ABI surface but are not enabled in Batch 4:

- `include_deleted != 0`
