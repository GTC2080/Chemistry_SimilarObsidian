<!-- Reason: This file freezes the Phase 1 host-facing contract for the public read/query ABI so hosts do not have to infer semantics from tests or SQLite internals. -->

# Query Public Surface

Last updated: `2026-04-24`

## Scope

This document freezes the Phase 1 host-facing behavior for:

- `kernel_search_notes(...)`
- `kernel_search_notes_limited(...)`
- `kernel_query_tag_notes(...)`
- `kernel_query_tags(...)`
- `kernel_query_graph(...)`
- `kernel_query_backlinks(...)`
- `kernel_free_search_results(...)`
- `kernel_free_tag_list(...)`
- `kernel_free_graph(...)`

It is a contract for hosts.
It is not an internal implementation guide.

Legacy note-search status:

- `kernel_search_notes(...)` and `kernel_search_notes_limited(...)` are now `deprecated-but-supported`
- Track 1 search expansion does not add `snippet / pagination / filters / ranking` behavior to these legacy entry points
- new host-facing search expansion behavior is frozen separately in [search-query-contract.md](/E:/测试/Chemistry_Obsidian/kernel/docs/contracts/search-query-contract.md)

## Note Hit Result Contract

`kernel_search_notes(...)`, `kernel_search_notes_limited(...)`, `kernel_query_tag_notes(...)`, and `kernel_query_backlinks(...)` return `kernel_search_results`, which owns an array of `kernel_search_hit`.

Each hit exposes:

- `rel_path`: UTF-8 relative note path as stored by the kernel
- `title`: current parser-derived or filename-fallback title
- `match_flags`: search-only match metadata

Shared guarantees:

- result order is deterministic
- result order is `rel_path` ascending
- `limit` caps the number of returned hits when that function accepts a `limit`
- returned strings are kernel-owned and remain valid until `kernel_free_search_results(...)`
- `kernel_free_search_results(...)` is idempotent and leaves the struct empty

Overwrite and failure behavior:

- passing a non-null `out_results` is required
- query APIs may reuse the same `kernel_search_results` struct across calls
- if `out_results` already contains kernel-owned hits from a previous successful query, the next query call releases them before writing new output
- if a query call fails and `out_results` is non-null, it returns an empty result object:
  - `hits == nullptr`
  - `count == 0`

## Search Contract

`kernel_search_notes(...)` and `kernel_search_notes_limited(...)` use the same result structure and the same matching rules.
`kernel_search_notes(...)` is the unbounded form.
`kernel_search_notes_limited(...)` is the bounded form.

Input rules:

- `handle` must be non-null
- `query` must be non-null and not empty
- whitespace-only queries are invalid
- `limit` must be greater than zero for the limited entry point

Matching rules:

- query text is interpreted as one or more whitespace-delimited literal tokens
- all query tokens must match for a note to be returned
- hyphenated tokens are treated as literal text
- result ordering is `rel_path` ascending
- there is at most one hit per note

`match_flags` rules:

- `KERNEL_SEARCH_MATCH_TITLE` means all query tokens were found in the indexed title
- `KERNEL_SEARCH_MATCH_BODY` means all query tokens were found in the indexed body, excluding the duplicated leading heading line when that heading produced the title
- a hit may set both bits
- a hit may set exactly one bit

Phase 1 non-goals:

- no pagination contract beyond `limit`
- no ranking contract beyond deterministic `rel_path` ordering
- no snippet contract

## Tag Query Contract

`kernel_query_tag_notes(...)` returns notes whose parser-derived tag rows exactly match the supplied tag text.

Input rules:

- `handle` must be non-null
- `tag` must be non-null
- empty or whitespace-only tag input is invalid
- `limit` must be greater than zero

Tag semantics:

- pass the bare tag token without the leading `#`
- matching is against the stored parser-derived tag text
- result ordering is `rel_path` ascending

Result semantics:

- `title` is populated the same way as note search
- `match_flags` is always `KERNEL_SEARCH_MATCH_NONE`

## Tag Summary Contract

`kernel_query_tags(...)` returns the parser-derived tag catalog from the kernel index.
This is the only host-facing tag summary truth for the Tauri shell.

Input rules:

- `handle` must be non-null
- `limit` must be greater than zero
- `out_tags` must be non-null

Tag semantics:

- each `kernel_tag_record.name` is the bare stored tag text without the leading `#`
- each `kernel_tag_record.count` is the number of live notes carrying that exact tag
- tags are derived from the same parser output that feeds `kernel_query_tag_notes(...)`
- hierarchy is not stored as a second truth source; hosts that need a tag tree must derive it from returned tag names

Ordering and ownership:

- result ordering is deterministic: count descending, then tag name ascending
- returned strings are kernel-owned and remain valid until `kernel_free_tag_list(...)`
- `kernel_free_tag_list(...)` is idempotent and leaves the struct empty

## Graph Query Contract

`kernel_query_graph(...)` returns the note relationship graph derived from live kernel state.
This is the only host-facing graph truth for backlinks/tag/folder relationship visualization.

Input rules:

- `handle` must be non-null
- `note_limit` must be greater than zero
- `out_graph` must be non-null

Node semantics:

- each live note within the bounded catalog produces a non-ghost node
- `id` is the normalized relative note path
- `name` is the current parser-derived or filename-fallback title
- linked-but-missing wiki targets may appear as `ghost` nodes

Link semantics:

- `kind = "link"` represents a parser-derived wiki link
- `kind = "tag"` represents notes sharing an exact parser-derived tag
- `kind = "folder"` represents sibling or adjacent folder relationships used by the current graph surface
- duplicate undirected graph pairs are collapsed by the kernel before returning results

Ordering and ownership:

- node and link ordering is deterministic for regression comparison
- returned strings are kernel-owned and remain valid until `kernel_free_graph(...)`
- `kernel_free_graph(...)` is idempotent and leaves the struct empty

## Backlinks Query Contract

`kernel_query_backlinks(...)` returns source notes that currently link to the addressed target note.

Input rules:

- `handle` must be non-null
- `rel_path` must be a non-empty relative path
- absolute paths are invalid
- rooted paths are invalid
- any path containing `..` is invalid
- `limit` must be greater than zero

Path semantics:

- input paths are normalized with the same lexical relative-path normalization used elsewhere in the kernel
- hosts may pass either forward slashes or Windows separators in a valid relative path
- lookup is by normalized note relative path, not by raw title text

Result semantics:

- result ordering is `rel_path` ascending
- `title` is populated with the current source-note title
- `match_flags` is always `KERNEL_SEARCH_MATCH_NONE`

## Consistency Contract

These query surfaces read the kernel's derived SQLite state, but their observable semantics are frozen around disk truth:

- after a successful `kernel_write_note(...)`, stale search/tag/backlink rows for that note are replaced
- after startup recovery finishes, recovered disk truth replaces stale search/tag/backlink rows
- after rebuild finishes successfully, disk truth replaces stale search/tag/backlink rows
- `kernel_query_tags(...)`, `kernel_query_tag_notes(...)`, `kernel_query_graph(...)`, and `kernel_query_backlinks(...)` must all agree on the same parser-derived tag/wiki-link rows

Hosts should treat query results as stable only after the runtime has reached a healthy queryable state such as `READY`.

## Error Surface

These query APIs use `kernel_status.code` only.

Expected public error classes:

- `KERNEL_OK`
- `KERNEL_ERROR_INVALID_ARGUMENT`
- `KERNEL_ERROR_IO`

`KERNEL_ERROR_INVALID_ARGUMENT` covers the invalid-input cases listed above.
Query misses are not errors; they return `KERNEL_OK` with zero hits.
