<!-- Reason: This file gives the repository a single-page Phase 1 status snapshot so implementation progress and remaining scope stay visible without replaying the chat log. -->

# Kernel Phase 1 Status

Last updated: `2026-04-21`

## Purpose

This document is a short operational snapshot of the current Phase 1 kernel.
It is not a design spec and not a roadmap replacement.
Its job is to answer four questions quickly:

1. What is already working?
2. What is closed for the current scope?
3. What remains outside the current scope?
4. What is the next mainline?

## Current Read

The kernel is no longer a skeleton-only repo.
It now has a working local runtime with:

- single-vault session open/close
- note read/write
- sidecar recovery journal
- SQLite-derived local state
- minimal parser-derived metadata
- minimal FTS-backed search
- Windows watcher-driven incremental refresh

Current maturity: `Phase 1 host-stable baseline`

That means:

- the architectural spine is real
- the core runtime loop is real
- crash/recovery/search/watch/rebuild/query/attachment/diagnostics paths exist as formal runtime capabilities
- the host-facing runtime contract is now frozen tightly enough that hosts can depend on it without guessing hidden state
- the remaining work is now future expansion beyond the current Phase 1 baseline, not unfinished closure work inside it

## Phase 1 Readiness

Current judgment: `host-stable baseline reached`.

What is already strong enough:

- one-vault headless runtime is real
- runtime state contract, recovery matrix, watcher lifecycle, rebuild lifecycle, and diagnostics support bundle are all repository-local and regression-backed
- host-facing read surfaces now exist for search, tags, backlinks, and attachments without requiring direct SQLite access
- full Debug build is green
- full `ctest` suite is green
- the repository now has an explicit release checklist in `docs/release-checklist.md`

What still remains outside the current baseline:

- richer search behavior such as snippets, ranking, filters, or pagination
- richer attachment metadata and content processing
- versioned diagnostics schema work beyond the current JSON support bundle
- broader platform/product expansion such as multi-vault, sync, plugins, PDF depth, or chemistry-specific layers

Latest benchmark evidence:

- startup benchmark:
  - `clean_baseline_ms=3465`
  - `clean_threshold_ms=6000`
  - `clean_observed_ms=2318`
  - `recovery_baseline_ms=3865`
  - `recovery_threshold_ms=7000`
  - `recovery_observed_ms=2318`
- io benchmark:
  - `io_roundtrip_baseline_ms=168`
  - `io_roundtrip_threshold_ms=500`
  - `io_roundtrip_observed_ms=308`
  - `external_create_baseline_ms=289`
  - `external_create_threshold_ms=800`
  - `external_create_observed_ms=318`
- rebuild benchmark:
  - `rebuild_baseline_ms=15885`
  - `rebuild_threshold_ms=22000`
  - `rebuild_observed_ms=14406`
- query benchmark:
  - `tag_query_baseline_ms=54`
  - `tag_query_threshold_ms=150`
  - `tag_query_observed_ms=67`
  - `title_query_baseline_ms=22`
  - `title_query_threshold_ms=100`
  - `title_query_observed_ms=25`
  - `body_query_baseline_ms=21`
  - `body_query_threshold_ms=100`
  - `body_query_observed_ms=22`
  - `backlink_query_baseline_ms=94`
  - `backlink_query_threshold_ms=250`
  - `backlink_query_observed_ms=101`

Short read:

- the kernel is no longer proving only local subsystem correctness
- it is now proving a host-usable end-to-end runtime loop with frozen Phase 1 boundaries
- the current runtime contract is strong enough for hosts to integrate directly without guessing state semantics
- the remaining work is now future-scope expansion, not unresolved Batch 1-6 hardening

## Completed

### Runtime shape

- Single-process headless kernel
- C++20 internal implementation
- Minimal C ABI boundary
- Windows-first runtime path

### Vault and file IO

- Open one vault
- Close one vault
- Read one note
- Write one note with optimistic revision checks
- Start one background rebuild
- Query one background rebuild status snapshot
- Wait for one background rebuild with timeout
- Join one background rebuild
- Rebuild one vault index synchronously through a formal C ABI entrypoint
- Export one diagnostics snapshot synchronously through a formal C ABI entrypoint
- Allow empty-content notes
- No-op writes preserve revision semantics

### State and recovery

- Per-vault local `state_dir`
- `state.sqlite3` created automatically
- `recovery.journal` sidecar created and consumed
- Startup recovery from unfinished save records
- Torn-tail / truncated / CRC-bad journal suffix handling
- Temp-file cleanup during recovery
- Recovery truth source is the sidecar journal, not SQLite

### Storage

- SQLite schema in place
- `notes`
- `journal_state` as diagnostics mirror only
- `note_tags`
- `note_links`
- `attachments`
- `note_attachment_refs`
- `note_fts`
- note upsert/delete derived-state writes now run inside SQLite transactions

### Revision model

- `content_revision = v1:sha256(raw_note_bytes)`
- Conflict detection based on current on-disk bytes
- Rename/move does not change content revision

### Parser

- First ATX heading -> title
- `#tag` extraction
- `[[wikilink]]` extraction
- local attachment-ref extraction from markdown links / images
- local attachment-ref extraction from Obsidian embeds
- Alias stripping for wikilinks
- Title fallback to filename stem
- Parser preserves duplicate tags and links, preserving order

### Attachment indexing/runtime

- Attachment metadata is now registered in SQLite
- Note-to-attachment references are now persisted
- Note write / rewrite now replace stale attachment refs
- Rebuild now reconciles attachment missing state back to disk truth
- Incremental refresh now handles non-markdown attachment paths
- External attachment delete now marks `attachments.is_missing=1`
- Internal rename application now reconciles attachment old/new paths as missing/present without widening the public attachment surface

### Search

- FTS5-backed note search
- Search available through C ABI
- Limited note search available through C ABI
- Tag query available through C ABI
- Backlinks query available through C ABI
- Search hits now expose minimal `TITLE/BODY` match flags
- Tag/backlink queries now enforce invalid-input rejection for empty / whitespace-only / invalid-path / zero-limit requests
- Tag/backlink queries now support a minimal host-controlled `limit`
- Limited note search now rejects empty queries and `limit=0`
- Limited note search now supports a minimal host-controlled `limit`
- Title and body both searchable
- Search hits now distinguish title-only, body-only, and shared matches at a minimal host-facing level
- Filename-fallback title searchable
- Deterministic result order by `rel_path`
- One hit per note
- Literal-token normalization for common punctuation and hyphenated queries
- Whitespace-only query rejection

### Watcher and incremental refresh

- Win32 notify-buffer decode
- Raw event coalescing
- One-shot watch session
- Incremental refresh on create / modify / delete
- Overflow -> `FullRescan`
- Explicit watcher rename pairs now coalesce to an internal `RenamePath` action
- Recognized rename pairs now preserve note identity across rename/move
- Unrecognized rename/move patterns still degrade to the existing delete/refresh path
- Background watcher runtime auto-start on `kernel_open_vault`
- Initial background catch-up scan runs after `kernel_open_vault`
- `index_state` now uses `CATCHING_UP -> READY` during normal startup
- runtime state snapshots are now decoupled from long-running storage work
- `kernel_get_state(...)` can now observe in-progress `REBUILDING` without waiting for rebuild completion
- diagnostics export now preserves `REBUILDING` as the live runtime state during delayed rebuilds without inventing fault fields
- healthy `CATCHING_UP` now hides stale `indexed_note_count` until reconciliation completes
- diagnostics export now mirrors healthy `CATCHING_UP` with zero fault fields and `indexed_note_count=0`
- Runtime suppression of self-written note events
- watcher loop no longer holds the runtime-state mutex while sleeping behind explicit rebuild gates
- `index_state` reflects watcher runtime health at a minimal level
- Initial catch-up failure now degrades to `UNAVAILABLE` and is allowed to recover back to `READY` after a later successful retry
- repeated identical watcher poll faults now auto-recover back to `READY` without external work and collapse to a single watcher-fault history record
- repeated watcher poll faults now pass through an explicit retry backoff window before `READY` is restored, preventing tight retry spinning during transient watcher degradation
- `kernel_close(...)` now has explicit regression coverage proving it interrupts watcher fault backoff promptly instead of waiting through retry sleeps
- closing a vault now has explicit regression coverage proving the background watcher stops indexing until the next open/catch-up cycle
- closing a vault now has explicit regression coverage proving watcher directory handles are released and the vault directory can be renamed immediately after `kernel_close(...)`
- closing a vault during delayed initial catch-up now has explicit regression coverage proving interrupted catch-up work does not commit new derived-state rows before shutdown completes
- self-write suppression now has explicit regression coverage proving a later real external modify is still observed and indexed
- delayed initial catch-up now has explicit regression coverage proving an external create is not double-applied when catch-up hands off to normal watcher polling
- watcher integration now has explicit regression coverage proving a partial mid-batch apply interruption can leave unapplied disk-backed actions behind, and that reopen catch-up repairs those remaining actions back to disk truth
- a dedicated watcher lifecycle checklist now exists in `docs/watcher-lifecycle-checklist.md`

### Rebuild

- Formal synchronous rebuild entrypoint exists
- Formal background rebuild start/join entrypoints now exist
- Formal background rebuild timeout-wait entrypoint now exists
- Rebuild currently uses the existing markdown full-rescan path
- Rebuild restores disk truth into:
  - `notes`
  - `note_tags`
  - `note_links`
  - `note_fts`
- Rebuild drives a minimal `REBUILDING -> READY/UNAVAILABLE` state transition
- Background rebuild is single-instance; duplicate start requests are rejected
- Background rebuild now has explicit API regression coverage proving:
  - diagnostics export `rebuild_in_flight=true` while healthy rebuild work is still running
  - status query reports idle -> running -> succeeded across one background rebuild lifecycle
  - status query now exposes monotonic background rebuild generations so hosts can distinguish successive tasks
  - timeout-wait returns `KERNEL_ERROR_NOT_FOUND` on a fresh runtime with no rebuild task to wait on
  - status query exposes a non-zero `current_started_at_ns` while rebuild work is still in flight
  - status query clears `current_started_at_ns` back to zero once rebuild work completes
  - status query reports `current_generation` while running and advances `last_completed_generation` only after completion
  - status query preserves the previous completed result while the next rebuild task is already in flight
  - status query reports the last background rebuild failure result after a failed run
  - synchronous rebuild rejects with conflict while background rebuild is already in flight
  - join returns `KERNEL_ERROR_NOT_FOUND` on a fresh runtime with no rebuild task to join
  - joining a completed background rebuild is idempotent
  - timeout-wait returns `KERNEL_ERROR_TIMEOUT` while delayed rebuild work is still running
  - timeout-wait returns final success once rebuild completes within the wait window
- Rebuild failure and later successful retry are now both regression-tested
- Diagnostics export now exposes `rebuild_failed` when rebuild is the current degraded runtime cause
- Diagnostics export now exposes `rebuild_in_flight`
- Diagnostics export now exposes rebuild generation fields aligned with status-query task generations
- Diagnostics export now exposes `last_rebuild_result_code` so hosts/support do not need to parse the string result field

### Verification

- Debug build is green
- `ctest` is green
- Startup benchmark exists
- IO benchmark exists
- rebuild benchmark now exists
- rebuild benchmark now runs with mixed note/attachment data
- query benchmark now exists for tag/backlink query baselines
- query benchmark now records minimal title/body search timing
- External-create watcher runtime cost is now benchmarked
- benchmark executables now enforce fixed baselines and regression thresholds instead of printing timings only
- named build targets now exist for verification:
  - `kernel_benchmark_gates`
  - `kernel_phase_gate`
- Fresh benchmark snapshot on `2026-04-21`:
  - startup: `clean_observed_ms=2318`, `recovery_observed_ms=2318`
  - io: `io_roundtrip_observed_ms=308`, `external_create_observed_ms=318`
  - rebuild: `note_count=64`, `attachment_count=64`, `rebuild_iterations=25`, `rebuild_observed_ms=14406`
  - query: `note_count=64`, `iterations=200`, `tag_query_observed_ms=67`, `title_query_observed_ms=25`, `body_query_observed_ms=22`, `backlink_query_observed_ms=101`
- One Phase 1 smoke regression now covers:
  - open
  - write
  - search
  - watcher-driven external modify
  - rebuild
  - diagnostics export

## Closed For Current Scope

These areas are closed for the current Phase 1 baseline.
The notes below call out deliberate future expansion, not missing closure work.

### Runtime state model

- `session_state` exists
- `index_state` exists
- `index_state` now reports:
  - `CATCHING_UP` during initial background reconciliation
  - `READY` after a healthy watcher/catch-up runtime
  - `UNAVAILABLE` after watcher runtime failures
  - `REBUILDING` during synchronous or background rebuild
- `CATCHING_UP` is now explicitly regression-tested through `kernel_get_state(...)`
- healthy `CATCHING_UP` now explicitly suppresses stale `indexed_note_count` until reconciliation completes
- `REBUILDING` is now explicitly regression-tested through `kernel_get_state(...)`
- `REBUILDING` is now explicitly regression-tested through diagnostics export as a non-fault in-progress state
- `kernel_get_state(...)` now reads a lightweight runtime snapshot rather than blocking on long storage operations
- runtime-state contract is now frozen in docs/tests, including fixed `open -> CATCHING_UP -> READY`, `READY -> UNAVAILABLE -> READY`, and `READY -> REBUILDING -> READY/UNAVAILABLE` behavior
- Future expansion, not required for this baseline:
  - finer-grained progress states beyond the current four-state contract
  - higher-level UI/UX status mapping layered on top of the frozen runtime contract

### Rebuild story

- `FullRescan` exists as overflow reconciliation
- Formal rebuild entrypoint now exists
- Formal background rebuild start/join entrypoints now exist
- Formal background rebuild status-query entrypoint now exists
- Diagnostics now export `rebuild_in_flight` alongside last rebuild outcome
- Diagnostics now export `rebuild_current_started_at_ns` alongside the running rebuild generation
- Diagnostics now export `has_last_rebuild_result` so hosts/support can distinguish a fresh runtime from a completed-task runtime without inferring from sentinel codes
- Completed background rebuild results now remain readable across repeated `wait` / `join` calls until a newer task supersedes them
- `kernel_close(...)` now has explicit regression coverage proving it waits for an in-flight background rebuild to finish and persist disk-truth reconciliation before returning
- Diagnostics now export `last_recovery_outcome` and `last_recovery_detected_corrupt_tail`, giving startup-recovery crash-matrix coverage a minimal host-visible surface
- Diagnostics now export `last_recovery_at_ns`, giving hosts/support a stable timestamp for the latest startup-recovery outcome
- Startup recovery now has explicit regression coverage proving that sidecar journal truth overrides conflicting `journal_state` mirror rows
- Startup recovery now has explicit regression coverage proving that a pre-replace crash keeps the old target file as disk truth instead of leaking staged temp content into recovered derived state
- Startup recovery now has explicit regression coverage proving that a post-replace crash still rebuilds derived state from the replaced target even when the temp path has already been cleaned up
- Reopening the vault now has explicit regression coverage proving that initial catch-up repairs stale derived state that could be left behind by an interrupted rebuild
- Background rebuild now has explicit regression coverage proving that an interruption after the file-refresh phase can still leave stale ghost rows behind, and that reopen catch-up repairs those rows back to disk truth
- Startup recovery now distinguishes `clean_startup`, `recovered_pending_saves`, and `cleaned_temp_only_pending_saves` in diagnostics
- Startup recovery now has explicit regression coverage proving that recovered markdown attachment refs and attachment metadata are reconciled back to disk truth
- Startup recovery plus reopen catch-up now has explicit regression coverage proving that deleted-note drift is removed and stale search/derived rows are cleared back to disk truth
- Reopen catch-up now has explicit regression coverage proving that closed-window attachment deletes are reconciled into `attachments.is_missing` while preserving note attachment refs
- Startup recovery now has explicit regression coverage proving that recovered note attachment refs are preserved even when the referenced attachment is missing, with `attachments.is_missing` reconciled from disk truth
- Close during watcher-fault backoff now has explicit regression coverage proving that external deletes are left for reopen catch-up, which then clears stale search and derived rows back to disk truth
- Close during watcher-fault backoff now also has explicit regression coverage proving that external modifies are left for reopen catch-up, which then restores disk-backed note metadata and search rows
- Close during watcher-fault backoff now also has explicit regression coverage proving that external creates are left for reopen catch-up, which then persists disk-backed note metadata and search rows
- Close during watcher-fault backoff now also has explicit regression coverage proving that closed-window attachment creates are left for reopen catch-up, which then reconciles `attachments.is_missing` back to present state without dropping note attachment refs
- Close during watcher-fault backoff now also has explicit regression coverage proving that closed-window attachment deletes are left for reopen catch-up, which then reconciles `attachments.is_missing` back to missing state without dropping note attachment refs
- Close during watcher-fault backoff now also has explicit regression coverage proving that closed-window attachment modifies are left for reopen catch-up, which then reconciles attachment metadata back to current disk truth without dropping note attachment refs
- Recovery/crash-matrix contracts are now collected in `docs/recovery-crash-matrix-checklist.md`
- Recovery/crash-matrix closure is now tracked through that checklist, which currently has no named remaining Batch 3 gaps
- Future expansion, not required for this baseline:
  - richer asynchronous rebuild semantics beyond the current start/join surface
  - richer rebuild progress/reporting

### Rename / move semantics

- Explicitly recognized watcher rename pairs now preserve note identity across rename/move
- Recognized rename pairs now also stay on the continuity path when the new path immediately emits follow-up modify noise in the same watcher window
- Internal rename application now also covers attachment old/new path reconciliation
- Rename/move degradation is now stable and diagnosable:
  - unpaired `rename-old` degrades to delete with a frozen fallback reason
  - unpaired `rename-new` degrades to refresh with a frozen fallback reason
- Future expansion, not required for this baseline:
  - stronger path-to-note continuity semantics outside the current explicit rename path

### Diagnostics

- Formal synchronous diagnostics export entrypoint exists
- Diagnostics export currently writes a JSON snapshot with:
  - session/index state
  - index fault reason/code/timestamp for runtime failures
  - bounded fault history for recent runtime failures
  - bounded `recent_events` support timeline
  - last rebuild result/timestamp/duration
  - indexed note count
  - latest continuity fallback reason/timestamp
  - logger backend identity
  - pending recovery count
  - vault/state/storage/journal paths
- Diagnostics now function as a single-file support bundle for current health plus recent history
- Diagnostics smoke is now part of the repository-local release checklist
- Future expansion, not required for this baseline:
  - versioned public schema guarantees beyond the current support-bundle fields
  - richer log export beyond the current logger backend identity
  - broader fault/event detail beyond the currently-tested startup, watcher, rebuild, and continuity paths

### Search surface

- Minimal search works
- Minimal tag/backlink read APIs now exist for hosts, using the same read-only result style as note search
- Public query-surface semantics are now frozen in `docs/query-public-surface.md`
- Tag/backlink query boundary is now tighter:
  - invalid input is rejected explicitly
  - `limit` is supported
  - ordering remains deterministic by `rel_path`
- Future expansion, not required for this baseline:
  - snippet / ranking / filters / pagination

### Attachment surface

- Minimal attachment indexing/runtime is now active
- Internal rename application now has explicit regression coverage for attachment old/new path reconciliation
- Minimal public attachment read surface now exists for hosts:
  - `kernel_list_note_attachments(...)`
  - `kernel_get_attachment_metadata(...)`
  - `kernel_free_attachment_refs(...)`
- Diagnostics now export host-facing attachment counts:
  - `attachment_count`
  - `missing_attachment_count`
- Future expansion, not required for this baseline:
  - richer attachment metadata beyond path / size / mtime / missing
  - non-note attachment-specific diagnostics beyond aggregate counts

## Outside Current Scope

These remain outside the implemented kernel today.

- PDF depth features
- AI-related features
- Chemical object model
- Plugin system
- Cloud sync
- Multi-vault runtime
- Team collaboration
- Mobile support
- Query language beyond current literal FTS surface
- Versioned diagnostics schema beyond the current JSON support bundle
- Background task scheduler beyond the current watcher thread and rebuild task runtime

## Next Mainline

If we continue after freezing this host-stable Phase 1 baseline, the most reasonable next line is:

1. Decide whether diagnostics should become a versioned public support-bundle contract
2. Expand host-facing read surfaces only where the current baseline is intentionally narrow
3. Plan any broader Phase 2 work on top of the now-frozen runtime/recovery/rebuild/watcher foundation

In practice, that means the next implementation priority should be:

- choose which support-bundle fields deserve versioned stability guarantees
- decide whether search/attachment richness belongs in this line or a later one
- keep new work behind the existing release checklist and benchmark gates

## What Should Not Happen Next

Do not treat these as next steps:

- plugin system work
- sync work
- PDF/AI/chemistry expansion
- search ranking polish
- UI-layer work
- reopening the closed runtime/rebuild/diagnostics contract without a concrete incompatibility reason

## Phase 1 Baseline

This kernel now qualifies as a `Phase 1 host-stable baseline` because all of the following are currently true:

- runtime state transitions are explicit enough for a host to observe and react to
- in-progress rebuild is host-observable through `kernel_get_state(...)`
- rebuild is a first-class supported path with tested success/failure/recovery behavior
- watcher/runtime faults can be surfaced cleanly to a host through diagnostics export
- diagnostics export now covers the real health/history cases this line currently relies on, including recent events and rebuild duration
- search/tag/backlink/attachment public read surfaces are frozen tightly enough for host integration
- recovery/crash matrix and watcher lifecycle expectations are frozen in repository checklists
- current build, tests, and benchmarks are green on a fresh verification run
- the repo now has a named phase-gate entrypoint plus a repository-local release checklist instead of relying on remembered manual command chains
