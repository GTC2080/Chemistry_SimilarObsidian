# State / Recovery / Storage Stabilization Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Stabilize the Phase 1 `state / recovery / storage` backbone so vault state, sidecar recovery, and SQLite schema behave predictably. This plan originally stopped before parser/search; those follow-on steps have now landed and are recorded below instead of leaving the document stale.

**Architecture:** Keep the runtime truth split frozen: vault files are the source of truth, `recovery.journal` is the recovery truth source, and `state.sqlite3` is derived local state plus diagnostics. The original focus was startup recovery, sidecar cleanup, and SQLite-backed state reporting; follow-on parser and search work now sits on top of that frozen base.

**Tech Stack:** C++20, CMake, MSVC, SQLite3 amalgamation, custom C ABI tests, Windows filesystem primitives

---

## Current Status

Already landed and should be preserved:

- Minimal C ABI exists in `include/kernel/c_api.h`
- `state_dir` derivation exists in `src/vault/state_paths.cpp`
- SQLite state schema exists in `src/storage/storage.cpp`
- Sidecar `recovery.journal` framing exists in `src/recovery/journal.cpp`
- `kernel_get_state` already reports layered `session_state + index_state`
- Startup recovery now scans the sidecar journal instead of trusting SQLite mirrors
- Parser-derived `title / note_tags / note_links` persistence has landed
- `note_fts` and minimal FTS-backed search have landed
- Minimal search C ABI has landed in `include/kernel/c_api.h`
- Minimal rebuild C ABI has landed in `include/kernel/c_api.h`
- Minimal diagnostics export C ABI has landed in `include/kernel/c_api.h`
- Minimal watcher pipeline has landed:
  - raw Win32 notify buffer decode
  - event coalescing
  - one-shot watch session open/close/poll
  - watcher -> refresh integration
  - overflow -> full rescan reconciliation
  - background watcher runtime auto-start on `kernel_open_vault`
  - test-only one-shot watcher poll fault injection seam for runtime-state regression coverage

The tasks below capture the original stabilization phase. Additional parser/search progress is summarized in `Follow-On Progress` and `Phase Gate Evidence`.

## File Map

- Modify: `E:\测试\Chemistry_Obsidian\src\core\kernel.cpp`
  - Keep runtime orchestration thin; call storage and recovery helpers only.
- Modify: `E:\测试\Chemistry_Obsidian\src\recovery\journal.h`
  - Freeze startup recovery helper API and sidecar cleanup entrypoints.
- Modify: `E:\测试\Chemistry_Obsidian\src\recovery\journal.cpp`
  - Harden valid-prefix parsing, unfinished-save recovery, and cleanup rewrite.
- Modify: `E:\测试\Chemistry_Obsidian\src\storage\storage.h`
  - Keep only storage APIs still justified by this phase.
- Modify: `E:\测试\Chemistry_Obsidian\src\storage\storage.cpp`
  - Preserve schema v1 behavior and note metadata upsert rules used by recovery.
- Modify: `E:\测试\Chemistry_Obsidian\src\platform\platform.h`
  - Keep file/journal primitives narrow and consistent.
- Modify: `E:\测试\Chemistry_Obsidian\src\platform\windows\platform_windows.cpp`
  - Keep Windows-specific file operations behind the platform seam.
- Modify: `E:\测试\Chemistry_Obsidian\tests\api\kernel_api_tests.cpp`
  - Add or adjust regression coverage for recovery and storage invariants.
- Modify: `E:\测试\Chemistry_Obsidian\benchmarks\startup\startup_benchmark.cpp`
  - Measure open/close overhead after recovery and storage stabilization.
- Modify: `E:\测试\Chemistry_Obsidian\benchmarks\io\io_benchmark.cpp`
  - Measure minimal write/read roundtrip cost after stabilization.

## Out of Scope

Historical out-of-scope items for the original stabilization phase:

- Parser
- Search
- FTS5 wiring
- Watcher
- Rename/move/delete state machine
- Attachment indexing
- UI or CLI product behavior

## Follow-On Progress

After the original stabilization tasks completed, the kernel advanced in the following controlled steps:

- Minimal parser landed:
  - first ATX heading becomes title
  - `#tag` extraction
  - `[[wikilink]]` extraction with alias stripping
  - title and wikilink trimming rules covered by tests
- Derived parser state landed in SQLite:
  - `notes.title`
  - `note_tags`
  - `note_links`
- Minimal FTS search landed:
  - `note_fts(title, body)`
  - write / rewrite / no-op / startup recovery all keep FTS rows consistent
- Minimal search C ABI landed:
  - synchronous read-only search
  - result payload currently exposes `rel_path + title + match_flags`
  - limited search now has a dedicated host-facing C ABI entrypoint
- Minimal tag/backlink query C ABI landed:
  - `kernel_query_tag_notes(...)`
  - `kernel_query_backlinks(...)`
  - both reuse the existing read-only `kernel_search_results` result shape
  - both now accept a minimal host-controlled `limit`
  - invalid input is explicitly rejected (`empty/whitespace tag`, `invalid rel_path`, `limit=0`)
- Minimal attachment indexing/runtime landed:
  - parser now extracts local attachment refs from markdown links/images and Obsidian embeds
  - SQLite now stores `attachments` and `note_attachment_refs`
  - note write / rewrite now replace stale attachment refs
  - rebuild now reconciles attachment missing state back to disk truth
  - incremental refresh now handles non-markdown attachment paths and marks deleted attachments missing
  - internal rename application now reconciles attachment old/new paths as missing/present
- Minimal rebuild C ABI landed:
  - synchronous `kernel_rebuild_index(...)`
  - rebuild reuses the existing markdown full-rescan path
- Minimal diagnostics export C ABI landed:
  - synchronous `kernel_export_diagnostics(...)`
  - export currently writes a JSON runtime snapshot to a caller-provided file path
- Search semantics now covered by tests:
  - body hits
  - title hits
  - title-only vs body-only match flags
  - filename-fallback title hits
  - hyphenated literal queries
  - whitespace-only query rejection
  - multi-token literal queries with extra whitespace
  - deterministic `rel_path` ordering
  - one hit per note even when a term repeats inside the note
  - limited search invalid-input rejection and result capping
- Tag/backlink semantics now covered by tests:
  - tag query returns matching notes ordered by `rel_path`
  - backlinks query returns matching source notes ordered by `rel_path`
  - invalid tag/backlink inputs are rejected explicitly
  - `limit` caps tag/backlink results without changing ordering
  - rewrite removes stale tag/backlink hits
  - rebuild restores disk-truth tag/backlink hits
  - startup recovery restores recovered tag/backlink hits
- Watcher semantics now covered by tests:
  - create / modify / delete raw coalescing
  - overflow -> `FullRescan`
  - first-seen path ordering across independent watcher actions
  - Win32 `FILE_NOTIFY_INFORMATION` decode for add / modify / remove / rename-old / rename-new
  - one-shot watch session open / close / poll
  - watcher-driven create / modify / delete refresh
  - explicitly recognized watcher rename pairs now coalesce to an internal `RenamePath` action
  - recognized rename pairs now preserve note identity across rename/move
  - unrecognized rename/move patterns still degrade to the existing delete/refresh path
  - watcher-driven `FullRescan` reconciliation across mixed create / modify / delete changes
  - API-level background watcher auto-indexing for external create / modify / delete changes
  - runtime suppression of self-written note events so internal atomic saves do not reapply stale watcher actions
  - `kernel_get_state.index_state` now reflects watcher runtime health:
    - `READY` after successful watcher startup
    - `UNAVAILABLE` if the watcher runtime hits poll/apply errors
    - `READY` again after later successful watcher iterations
  - API-level tests now cover watcher runtime degradation and recovery:
    - forced watcher poll fault drives `index_state` to `UNAVAILABLE`
    - later successful watcher work drives `index_state` back to `READY`
  - API-level rebuild tests now cover reconciliation of stale SQLite-derived state back to disk truth
- API-level diagnostics export tests now cover:
  - normal JSON snapshot export
  - watcher-fault `index_state` visibility inside exported diagnostics
  - bounded fault-history retention after successful recovery
  - last rebuild result / timestamp export

### Task 1: Freeze Startup Recovery Semantics

**Files:**
- Modify: `E:\测试\Chemistry_Obsidian\tests\api\kernel_api_tests.cpp`
- Modify: `E:\测试\Chemistry_Obsidian\src\recovery\journal.h`
- Modify: `E:\测试\Chemistry_Obsidian\src\recovery\journal.cpp`
- Modify: `E:\测试\Chemistry_Obsidian\src\core\kernel.cpp`

- [x] **Step 1: Write failing recovery tests for the final startup semantics**

Add or update tests so reopen behavior proves:
- dangling `SAVE_BEGIN` is consumed on `kernel_open_vault`
- target file wins if present
- stale temp file is deleted
- `pending_recovery_ops` becomes `0` after successful startup recovery
- journal cleanup leaves an empty sidecar file or an empty valid-prefix payload list

- [x] **Step 2: Run the test binary to verify the new expectations fail first**

Run:

```bat
cmd /c "call E:\Dev\bin\kernel-dev-x64.cmd >nul && cmake --build E:\测试\Chemistry_Obsidian\out\build --config Debug --target kernel_api_tests && E:\测试\Chemistry_Obsidian\out\build\tests\Debug\kernel_api_tests.exe"
```

Expected:
- `kernel_api_tests.exe` exits non-zero
- failure text points at startup recovery behavior, not build errors

- [x] **Step 3: Implement the minimal startup recovery path**

Implementation constraints:
- parse only the valid prefix of `recovery.journal`
- reconstruct unfinished saves from `SAVE_BEGIN` without matching `SAVE_COMMIT`
- if target file exists, accept it as truth and upsert `notes`
- if temp file exists, remove it
- compact the sidecar to an empty journal once recovery resolves all unfinished operations
- keep SQLite `journal_state` diagnostic-only

- [x] **Step 4: Re-run the focused test binary**

Run the same command as Step 2.

Expected:
- exit code `0`
- no Debug CRT popup

- [ ] **Step 5: Commit**

```bash
git add tests/api/kernel_api_tests.cpp src/recovery/journal.h src/recovery/journal.cpp src/core/kernel.cpp
git commit -m "feat: recover unfinished saves on vault open"
```

### Task 2: Harden Sidecar Valid-Prefix and Cleanup Rules

**Files:**
- Modify: `E:\测试\Chemistry_Obsidian\tests\api\kernel_api_tests.cpp`
- Modify: `E:\测试\Chemistry_Obsidian\src\recovery\journal.cpp`
- Modify: `E:\测试\Chemistry_Obsidian\src\platform\platform.h`
- Modify: `E:\测试\Chemistry_Obsidian\src\platform\windows\platform_windows.cpp`

- [x] **Step 1: Write failing tests for torn-tail and partial-record behavior**

Add tests covering:
- bad magic after a valid prefix
- truncated payload length
- CRC mismatch on the tail record
- malformed tail record after one valid `SAVE_BEGIN`

Each test should prove:
- earlier valid records are still honored
- invalid suffix is ignored
- startup recovery does not crash or report spurious pending operations after cleanup

- [x] **Step 2: Run the focused test binary and verify it fails on the new cases**

Run:

```bat
cmd /c "call E:\Dev\bin\kernel-dev-x64.cmd >nul && E:\测试\Chemistry_Obsidian\out\build\tests\Debug\kernel_api_tests.exe"
```

Expected:
- failure mentions torn-tail or invalid suffix handling

- [x] **Step 3: Implement minimal valid-prefix and cleanup hardening**

Implementation constraints:
- stop parsing at the first invalid record
- never read beyond the valid prefix
- treat the invalid suffix as torn tail
- rewrite the sidecar from resolved survivors only
- keep platform helpers generic; framing logic stays in recovery code

- [x] **Step 4: Re-run the focused test binary**

Run the same command as Step 2.

Expected:
- all recovery tests pass

- [ ] **Step 5: Commit**

```bash
git add tests/api/kernel_api_tests.cpp src/recovery/journal.cpp src/platform/platform.h src/platform/windows/platform_windows.cpp
git commit -m "test: harden torn-tail recovery journal handling"
```

### Task 3: Lock Storage v1 to Runtime Reality

**Files:**
- Modify: `E:\测试\Chemistry_Obsidian\tests\api\kernel_api_tests.cpp`
- Modify: `E:\测试\Chemistry_Obsidian\src\storage\storage.h`
- Modify: `E:\测试\Chemistry_Obsidian\src\storage\storage.cpp`
- Modify: `E:\测试\Chemistry_Obsidian\src\core\kernel.cpp`

- [x] **Step 1: Write failing tests for storage invariants used by runtime state**

Add or update tests so they prove:
- `indexed_note_count` matches `notes WHERE is_deleted=0`
- restart after recovered save leaves one live note row
- `journal_state` may mirror recovery events, but `kernel_get_state` must not depend on it
- schema v1 reopen is idempotent

- [x] **Step 2: Run the focused test binary to verify one of the new checks fails first**

Run:

```bat
cmd /c "call E:\Dev\bin\kernel-dev-x64.cmd >nul && E:\测试\Chemistry_Obsidian\out\build\tests\Debug\kernel_api_tests.exe"
```

Expected:
- failure text names a storage invariant, not a parser/search feature

- [x] **Step 3: Implement the minimal storage cleanup**

Implementation constraints:
- remove or quarantine runtime code paths that still imply SQLite is recovery truth
- preserve `notes` upsert semantics used by recovery
- keep schema at `user_version = 1`
- do not add new tables

- [x] **Step 4: Re-run the focused test binary**

Run the same command as Step 2.

Expected:
- storage invariants pass

- [ ] **Step 5: Commit**

```bash
git add tests/api/kernel_api_tests.cpp src/storage/storage.h src/storage/storage.cpp src/core/kernel.cpp
git commit -m "refactor: align storage v1 with runtime state semantics"
```

### Task 4: Freeze This Phase with End-to-End Verification

**Files:**
- Modify: `E:\测试\Chemistry_Obsidian\benchmarks\startup\startup_benchmark.cpp`
- Modify: `E:\测试\Chemistry_Obsidian\benchmarks\io\io_benchmark.cpp`
- Modify: `E:\测试\Chemistry_Obsidian\tests\api\kernel_api_tests.cpp`

- [x] **Step 1: Add the smallest benchmarks that reflect this phase**

Cover only:
- open/close vault with empty state
- open/close vault with one recovered unfinished save
- write/read one note roundtrip

- [x] **Step 2: Run full phase verification**

Run:

```bat
cmd /c "call E:\Dev\bin\kernel-dev-x64.cmd >nul && cmake --build E:\测试\Chemistry_Obsidian\out\build --config Debug"
cmd /c "call E:\Dev\bin\kernel-dev-x64.cmd >nul && ctest --test-dir E:\测试\Chemistry_Obsidian\out\build -C Debug --output-on-failure"
cmd /c "call E:\Dev\bin\kernel-dev-x64.cmd >nul && E:\测试\Chemistry_Obsidian\out\build\benchmarks\startup\Debug\kernel_startup_benchmark.exe"
cmd /c "call E:\Dev\bin\kernel-dev-x64.cmd >nul && E:\测试\Chemistry_Obsidian\out\build\benchmarks\io\Debug\kernel_io_benchmark.exe"
```

Expected:
- build exit code `0`
- `ctest` reports `100% tests passed`
- both benchmark executables run without crashing

- [x] **Step 3: Record the phase gate**

Confirm all of the following before moving on:
- no test depends on parser/search behavior
- `kernel_get_state` reflects runtime truth paths only
- `recovery.journal` is consumable and compactable
- `state.sqlite3` schema v1 is stable across reopen

- [ ] **Step 4: Commit**

```bash
git add tests/api/kernel_api_tests.cpp benchmarks/startup/startup_benchmark.cpp benchmarks/io/io_benchmark.cpp
git commit -m "test: freeze state recovery storage phase gate"
```

## Acceptance Criteria

This phase is complete only when:

- `kernel_open_vault` can recover unfinished save records without SQLite being the recovery authority
- `kernel_get_state` reports note count from SQLite and recovery status from the sidecar recovery path
- stale temp files are removed during startup recovery
- the sidecar journal is compacted after successful recovery
- all current tests pass in Debug build through `ctest`
- the two minimal benchmarks run successfully
- the original stabilization layer remains valid even after parser/search are layered on top

## Execution Notes

- Always run build and `ctest` serially on Windows. Do not run them in parallel.
- Treat any attempt to add parser/search/FTS in this phase as scope creep and reject it.
- Keep commits narrow: one recovery behavior or one storage invariant per commit.

## Phase Gate Evidence

Last verification run: `2026-04-21`

- Build:
  - `cmake --build E:\测试\Chemistry_Obsidian\out\build --config Debug`
  - Result: success
- Tests:
  - `ctest --test-dir E:\测试\Chemistry_Obsidian\out\build -C Debug --output-on-failure`
  - Result: `100% tests passed, 0 tests failed out of 4`
- Startup benchmark:
  - `E:\测试\Chemistry_Obsidian\out\build\benchmarks\Debug\kernel_startup_benchmark.exe`
  - Result: `clean_baseline_ms=3465 clean_threshold_ms=6000 clean_observed_ms=2318 recovery_baseline_ms=3865 recovery_threshold_ms=7000 recovery_observed_ms=2318 startup_benchmark clean_iterations=100 recovery_iterations=25 gate_passed=true`
- IO benchmark:
  - `E:\测试\Chemistry_Obsidian\out\build\benchmarks\Debug\kernel_io_benchmark.exe`
  - Result: `io_roundtrip_baseline_ms=168 io_roundtrip_threshold_ms=500 io_roundtrip_observed_ms=308 external_create_baseline_ms=289 external_create_threshold_ms=800 external_create_observed_ms=318 io_benchmark iterations=100 external_create_iterations=25 gate_passed=true`
- Rebuild benchmark:
  - `E:\测试\Chemistry_Obsidian\out\build\benchmarks\Debug\kernel_rebuild_benchmark.exe`
  - Result: `rebuild_baseline_ms=15885 rebuild_threshold_ms=22000 rebuild_observed_ms=14406 rebuild_benchmark note_count=64 attachment_count=64 rebuild_iterations=25 gate_passed=true`
- Query benchmark:
  - `E:\测试\Chemistry_Obsidian\out\build\benchmarks\Debug\kernel_query_benchmark.exe`
  - Result: `tag_query_baseline_ms=54 tag_query_threshold_ms=150 tag_query_observed_ms=67 title_query_baseline_ms=22 title_query_threshold_ms=100 title_query_observed_ms=25 body_query_baseline_ms=21 body_query_threshold_ms=100 body_query_observed_ms=22 backlink_query_baseline_ms=94 backlink_query_threshold_ms=250 backlink_query_observed_ms=101 query_benchmark note_count=64 iterations=200 gate_passed=true`

Current phase-gate reading:

- `kernel_open_vault` can recover unfinished saves from sidecar journal without SQLite acting as recovery authority
- `kernel_get_state` reads note count from SQLite-derived state and recovery count from sidecar recovery state
- stale temp files are removed during startup recovery
- `recovery.journal` is compacted after successful recovery
- current SQLite schema reopens idempotently
- note upsert/delete writes now execute inside SQLite transactions, reducing half-written derived-state exposure
- parser-derived title/tag/link persistence is active
- FTS-backed search and minimal search C ABI are active
- a formal synchronous rebuild entrypoint is now active
- minimal background rebuild start/join entrypoints are now active:
  - `kernel_start_rebuild_index(...)`
  - `kernel_get_rebuild_status(...)`
  - `kernel_wait_for_rebuild(...)`
  - `kernel_join_rebuild_index(...)`
  - duplicate background rebuild starts now reject with conflict
- a formal synchronous diagnostics export entrypoint is now active
- watcher raw decode, coalescing, one-shot poll, and refresh integration are active
- watcher runtime now auto-starts on `kernel_open_vault`
- an initial background catch-up scan now runs on `kernel_open_vault`
- `kernel_get_state.index_state` now reports:
  - `CATCHING_UP` during initial catch-up
  - `READY` while healthy after catch-up
  - `UNAVAILABLE` after watcher runtime failures
  - `REBUILDING` during explicit rebuild
- delayed initial full-rescan tests now prove that `CATCHING_UP` is observable through `kernel_get_state(...)`
- delayed initial full-rescan tests now prove that healthy `CATCHING_UP` suppresses stale `indexed_note_count` until reconciliation completes
- delayed initial diagnostics tests now prove that healthy `CATCHING_UP` exports zero fault fields and zero indexed count until reconciliation completes
- delayed rebuild tests now prove that `REBUILDING` is observable through `kernel_get_state(...)`
- delayed rebuild diagnostics tests now prove that explicit rebuild keeps exporting `REBUILDING` as a non-fault in-progress state until rebuild completion
- background watcher lifecycle now has explicit API regression coverage proving:
  - `kernel_close(...)` stops indexing work until the next open/catch-up cycle
  - `kernel_close(...)` releases watcher handles so the vault directory can be renamed immediately
  - `kernel_close(...)` during delayed initial catch-up does not allow interrupted catch-up work to commit new derived-state rows before shutdown completes
  - self-write suppression does not swallow a later real external modify
  - delayed initial catch-up does not double-apply an external create when the runtime later transitions into normal watcher polling
  - repeated identical watcher poll faults now auto-recover back to `READY` without external work and collapse to a single watcher-fault history record
  - repeated watcher poll faults now remain degraded through an explicit retry backoff window before `READY` is restored
  - `kernel_close(...)` now interrupts watcher fault backoff promptly instead of waiting through retry sleeps
- watcher rebuild gating no longer sleeps while holding the runtime-state mutex, avoiding starvation of `kernel_get_state(...)` and diagnostics export during explicit rebuild
- `kernel_get_state(...)` now reads a lightweight runtime snapshot instead of waiting behind long storage operations like rebuild
- watcher runtime fault observability is now regression-tested through a one-shot internal poll fault seam used only by tests
- reopen-after-close drift is now regression-tested through a startup catch-up test that verifies closed-window external edits are reconciled back into search/state
- initial catch-up retry behavior is now regression-tested:
  - forced catch-up failure degrades `index_state` to `UNAVAILABLE`
  - clearing the injected failure allows the runtime to recover back to `READY`
- benchmark evidence now includes watcher runtime external-create indexing cost through `external_create_elapsed_ms`
- benchmark evidence now includes a dedicated rebuild timing loop through `kernel_rebuild_benchmark.exe`
- benchmark evidence now includes dedicated tag/backlink query timings through `kernel_query_benchmark.exe`
- benchmark executables now enforce fixed baselines and regression thresholds directly
- fixed benchmark baselines and gates now live in `docs/benchmark-baselines.md`
- named build targets now exist for phase verification:
  - `kernel_benchmark_gates`
  - `kernel_phase_gate`
- minimal attachment indexing/runtime is now active:
  - `attachments`
  - `note_attachment_refs`
  - parser-derived attachment refs from local links/images and embeds
  - rebuild and incremental refresh attachment missing-state reconciliation
- rebuild is now regression-tested against stale derived-state drift in SQLite
- diagnostics export is now regression-tested against live runtime state, including watcher fault visibility
- diagnostics export now includes and tests:
  - `index_fault_reason`
  - `index_fault_code`
  - `index_fault_at_ns`
  - `index_fault_history`
  - `last_rebuild_result`
  - `last_rebuild_at_ns`
  - distinction between initial catch-up failure, watcher poll failure, and rebuild failure
- rebuild runtime semantics are now regression-tested:
  - failed rebuild degrades `index_state` to `UNAVAILABLE`
  - diagnostics export surfaces `rebuild_failed`
  - later successful rebuild clears fault fields and restores `READY`
- background rebuild runtime semantics are now regression-tested:
  - start transitions runtime state to `REBUILDING`
  - status query reports idle -> running -> succeeded across one background rebuild lifecycle
  - status query now exposes monotonic rebuild generations so successive background tasks are distinguishable
  - timeout-wait returns `KERNEL_ERROR_NOT_FOUND` on a fresh runtime with no rebuild task to wait on
  - status query exposes a non-zero `current_started_at_ns` while a rebuild is actively running
  - status query clears `current_started_at_ns` back to zero after completion
  - status query reports `current_generation` while running and advances `last_completed_generation` only after completion
  - status query preserves the previous completed result while the next rebuild task is already in flight
  - status query reports the last background rebuild failure result after a failed run
  - timeout-wait returns `KERNEL_ERROR_TIMEOUT` while delayed rebuild work is still running
  - timeout-wait returns the final rebuild result once work completes inside the wait window
  - join returns the final rebuild result
  - join returns `KERNEL_ERROR_NOT_FOUND` on a fresh runtime with no rebuild task to join
  - diagnostics export now exposes `rebuild_in_flight`
  - diagnostics export now exposes rebuild generation fields aligned with the status-query task generations
  - diagnostics export now exposes `rebuild_current_started_at_ns` aligned with status-query task timing
- diagnostics export now exposes `has_last_rebuild_result`
- completed background rebuild results remain readable across repeated `wait` / `join` calls until superseded by a newer task
- `kernel_close(...)` now waits for an in-flight background rebuild to finish and persist its reconciliation before the handle is torn down
- diagnostics export now exposes `last_recovery_outcome`
- diagnostics export now exposes `last_recovery_detected_corrupt_tail`
- startup recovery is now regression-tested to prefer sidecar journal truth over conflicting `journal_state` mirror rows
- startup recovery is now regression-tested to keep old target truth when crash happens before target replace, proving staged temp content does not leak into recovered derived state
- startup recovery is now regression-tested to recover replaced target truth after temp cleanup, proving a missing temp path does not block post-replace recovery
- startup reopen is now regression-tested to repair stale derived-state drift that could be left behind by an interrupted rebuild
- interrupted background rebuild is now regression-tested to leave partial stale ghost rows before reopen, with reopen catch-up restoring disk truth afterward
- interrupted watcher apply is now regression-tested to leave later disk-backed actions unapplied after a mid-batch interruption, with reopen catch-up repairing the remaining actions afterward
- startup recovery diagnostics now distinguish `clean_startup`, `recovered_pending_saves`, and `cleaned_temp_only_pending_saves`
- diagnostics export now exposes `last_recovery_at_ns`
- startup recovery is now regression-tested to replace stale attachment refs and attachment metadata from disk truth
- startup recovery plus reopen catch-up is now regression-tested to remove deleted-note drift and clear stale search/derived rows back to disk truth
- reopen catch-up is now regression-tested to reconcile closed-window attachment deletes into attachment missing-state without dropping note attachment refs
- startup recovery is now regression-tested to preserve recovered note attachment refs while marking missing recovered attachments as `is_missing=1`
- close during watcher backoff is now regression-tested to leave external deletes for reopen catch-up, which then clears stale search/derived rows back to disk truth
- close during watcher backoff is now regression-tested to leave external modifies for reopen catch-up, which then restores disk-backed note metadata and search rows
- close during watcher backoff is now regression-tested to leave external creates for reopen catch-up, which then persists disk-backed note metadata and search rows
- close during watcher backoff is now regression-tested to leave closed-window attachment creates for reopen catch-up, which then reconciles attachment missing-state back to present without dropping note attachment refs
- close during watcher backoff is now regression-tested to leave closed-window attachment deletes for reopen catch-up, which then reconciles attachment missing-state back to missing without dropping note attachment refs
- close during watcher backoff is now regression-tested to leave closed-window attachment modifies for reopen catch-up, which then reconciles attachment metadata back to current disk truth without dropping note attachment refs
- recovery/crash-matrix contracts are now summarized in `docs/recovery-crash-matrix-checklist.md`
- diagnostics export now exposes `last_rebuild_result_code`
  - synchronous rebuild rejects with conflict while background rebuild is already in flight
  - joining a completed background rebuild remains a harmless no-op
- one Phase 1 alpha smoke regression now exercises the integrated path:
  - `kernel_open_vault`
  - `kernel_write_note`
  - `kernel_search_notes`
  - watcher-driven external modify reconciliation
  - `kernel_rebuild_index`
  - `kernel_export_diagnostics`
- current readiness reading: alpha baseline reached; remaining work is now post-alpha hardening of runtime state semantics, rebuild UX, and diagnostics depth
- overflow has a formal `FullRescan` path
- recognized rename pairs now preserve note identity, while unrecognized rename/move patterns still degrade to delete/refresh
