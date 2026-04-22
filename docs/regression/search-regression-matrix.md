<!-- Reason: This file freezes the Track 1 search regression matrix so search behavior grows by explicit contract instead of host inference. -->

# Search Regression Matrix

Last updated: `2026-04-21`

## Batch 1: Snippet

The repository must retain regression coverage for:

- body hit returns one note hit with `snippet_status=BODY_EXTRACTED`
- body snippet contains the matching body token
- body snippet excludes the leading title heading when that heading produced the title
- body snippet is plain text with collapsed whitespace
- body snippet respects the fixed maximum length
- title-only hit returns empty snippet with `snippet_status=TITLE_ONLY`
- legacy note-search ABI remains supported without new snippet fields
- rewrite replaces stale snippet content
- diagnostics export exposes the frozen search contract revision, backend, snippet mode, and snippet max length

## Batch 2: Pagination

The repository must retain regression coverage for:

- first page returns exact `total_hits`
- first page returns exact `has_more`
- middle page returns exact `total_hits`
- middle page preserves deterministic ordering
- last page returns exact `has_more=false`
- out-of-range page returns `count=0`, exact `total_hits`, and `has_more=false`
- repeated page requests over the same healthy snapshot return the same ordering
- rewrite updates paged results and exact `total_hits`
- rebuild repairs paged results and exact `total_hits`

## Batch 3: Filters

The repository must retain regression coverage for:

- `kind=note` supports `tag_filter + path_prefix` together
- `kind=note` keeps deterministic `rel_path` ordering under filters
- `kind=attachment` searches attachment paths only
- attachment hits return `title=basename(rel_path)`
- attachment hits return empty snippet with `snippet_status=NONE`
- attachment hits return `match_flags=PATH`
- missing attachments return `result_flags=ATTACHMENT_MISSING`
- `kind=all` returns notes first and attachments second under `REL_PATH_ASC`
- `kind=all` preserves group-internal `rel_path` ordering under `REL_PATH_ASC`
- `kind=all + tag_filter` returns attachments referenced by matching tagged notes only
- `path_prefix` applies to the result object's own `rel_path`
- rewrite updates filtered results exactly
- rebuild repairs filtered results exactly after derived-state drift
- diagnostics export exposes filter support, supported kinds, include_deleted disabled, attachment path-only behavior, and fixed `kind=all` ordering

## Batch 4: Ranking v1

The repository must retain regression coverage for:

- `NOTE + RANK_V1` boosts title hits ahead of body-only hits
- `NOTE + RANK_V1` boosts exact single-token tag hits ahead of otherwise equivalent plain body hits
- `NOTE + RANK_V1` falls back to `rel_path ASC` when rank buckets tie
- `ALL + RANK_V1` ranks the note branch first and appends attachments after ranked notes
- `ALL + RANK_V1` pagination remains stable across the note/attachment boundary
- rewrite updates ranked results exactly
- rebuild repairs ranked results exactly after derived-state drift
- diagnostics export exposes ranking support, supported ranking kinds, ranking tie-break, title-hit boost enabled, and single-token tag-boost boundary

## Request Boundary

The repository must retain regression coverage for:

- empty query rejected
- whitespace-only query rejected
- zero limit rejected
- over-max page limit rejected
- `kind=attachment + tag_filter` rejected
- invalid `path_prefix` rejected
- `ATTACHMENT + RANK_V1` rejected
- `include_deleted=1` rejected
- invalid expanded-search request clears stale output

## Empty Result Contract

The repository must retain regression coverage for:

- empty query result returns `count=0`, `total_hits=0`, `has_more=0`
- out-of-range page returns empty hits while preserving exact `total_hits`
