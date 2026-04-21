# Recovery Crash-Matrix Checklist

## Startup Recovery

- `recovery.journal` sidecar is the source of truth for unfinished-save recovery.
- Conflicting `journal_state` rows in SQLite must not override sidecar truth.
- If crash happens before target replace:
  - old target content remains disk truth
  - staged temp content must not leak into recovered derived state
- Valid journal prefixes must still recover when the tail is torn, truncated, or CRC-corrupt.
- Startup recovery must compact consumed journal state back to an empty valid journal.
- Startup recovery must remove stale temp files left by unfinished saves.
- If crash happens after target replace and temp cleanup but before full steady state:
  - replaced target content remains disk truth
  - recovery must rebuild derived state from the replaced target even when the temp path is already gone
- Diagnostics must export:
  - `last_recovery_outcome`
  - `last_recovery_detected_corrupt_tail`
  - `last_recovery_at_ns`
- Startup recovery outcomes must distinguish:
  - `clean_startup`
  - `recovered_pending_saves`
  - `cleaned_temp_only_pending_saves`

## Recovered Note State

- Recovered note content must replace stale derived note state:
  - title
  - tags
  - wikilinks
  - FTS rows
- Recovered markdown attachment refs must replace stale `note_attachment_refs`.
- Recovered attachment metadata must reconcile back to disk truth.
- Recovered note attachment refs must be preserved even when the referenced attachment is missing.
- Missing recovered attachments must be recorded with `attachments.is_missing=1`.

## Reopen Catch-Up

- Reopening the vault after closed-window external modify must reconcile note metadata and search rows back to disk truth.
- Reopening the vault after stale SQLite drift must repair:
  - title drift
  - tag drift
  - link drift
  - FTS drift
- Reopening after interrupted background rebuild must clear stale ghost rows that survive an interrupted delete sweep while preserving live disk-backed rows.
- Reopening after interrupted watcher apply must finish any unapplied disk-backed actions that were left behind after a partial mid-batch apply interruption.
- Reopening after closed-window deleted note drift must:
  - mark `notes.is_deleted=1`
  - clear stale tags
  - clear stale links
  - clear stale attachment refs
- Reopening after closed-window attachment delete must:
  - preserve `note_attachment_refs`
  - reconcile `attachments.is_missing=1`

## Watcher Backoff Shutdown

- If watcher poll faults degrade runtime to backoff, `kernel_close(...)` must not let old watcher work keep mutating derived state after close returns.
- If an external delete happens during watcher-fault backoff before close:
  - stale live rows may remain before reopen
  - reopen catch-up must clear stale search and derived rows
- If an external modify happens during watcher-fault backoff before close:
  - stale title/tag/link rows may remain before reopen
  - reopen catch-up must restore disk-backed metadata and search rows
- If an external create happens during watcher-fault backoff before close:
  - no new note row should be committed before reopen
  - reopen catch-up must persist the created note and derived rows
- If an external attachment create happens during watcher-fault backoff before close:
  - stale `attachments.is_missing=1` may remain before reopen
  - reopen catch-up must reconcile the attachment back to present without dropping note attachment refs
- If an external attachment delete happens during watcher-fault backoff before close:
  - stale `attachments.is_missing=0` may remain before reopen
  - reopen catch-up must reconcile the attachment back to missing without dropping note attachment refs
- If an external attachment modify happens during watcher-fault backoff before close:
  - stale attachment metadata may remain before reopen
  - reopen catch-up must reconcile the attachment metadata back to current disk truth without dropping note attachment refs

## Watcher Mid-Apply Interruption

- If watcher apply is interrupted after some actions commit but before the batch finishes:
  - partial derived state may remain before reopen
  - reopen catch-up must finish the unapplied disk-backed actions without regressing the already-committed ones

## Phase Gate

- `kernel_api_tests` must cover the startup-recovery and reopen-catch-up contracts above.
- `kernel_phase_gate` must remain green after any recovery/crash-matrix hardening change.

## Remaining Gaps

- No named recovery/crash-matrix gaps remain in the current Batch 3 checklist.
