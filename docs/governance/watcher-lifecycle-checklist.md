<!-- Reason: This file freezes the watcher lifecycle hardening checklist so shutdown, fault, catch-up, and recovery boundaries stay explicit after alpha. -->

# Watcher Lifecycle Checklist

Last updated: `2026-04-21`

## Purpose

This checklist freezes the watcher lifecycle contract that now exists in code and tests.
It is not a design document.
Its job is to make Phase 1 watcher/runtime boundaries auditable without replaying the chat log or reading scattered test names.

## Startup

- `kernel_open_vault(...)` starts exactly one background watcher runtime.
- Healthy startup begins in `CATCHING_UP`.
- Healthy `CATCHING_UP` exports zero fault fields.
- Healthy `CATCHING_UP` hides stale `indexed_note_count`.
- Successful initial catch-up transitions to `READY`.
- Initial catch-up failure transitions to `UNAVAILABLE`.
- Initial catch-up retry is allowed to recover back to `READY`.

## Normal Watching

- External create / modify / delete are incrementally reconciled.
- Explicit watcher rename pairs are coalesced to `RenamePath`.
- Recognized rename pairs preserve note identity.
- Recognized rename pairs remain on the continuity path even if the new path also emits immediate follow-up modify noise in the same watcher window.
- Unrecognized rename/move patterns degrade to delete/refresh.
- Degraded rename/move paths record a stable continuity fallback reason for diagnostics.
- Self-written note events are suppressed only for the matching revision window.
- A later real external modify must still be observed and indexed.
- Overflow degrades to `FullRescan`.

## Fault Handling

- Watcher poll faults transition runtime state to `UNAVAILABLE`.
- Watcher apply faults transition runtime state to `UNAVAILABLE`.
- Fault diagnostics export the current live reason / code / timestamp.
- Repeated identical watcher poll faults collapse to one history record.
- Repeated watcher poll faults auto-recover back to `READY` after a later healthy poll.
- Repeated watcher poll faults now pass through an explicit retry backoff window before `READY` is restored.
- Fault backoff must not busy-spin.

## Shutdown

- `kernel_close(...)` stops background indexing until the next open/catch-up cycle.
- `kernel_close(...)` releases watcher directory handles promptly.
- Vault directories can be renamed immediately after `kernel_close(...)`.
- `kernel_close(...)` during delayed initial catch-up must not allow interrupted catch-up work to commit new derived-state rows before shutdown completes.
- `kernel_close(...)` must interrupt watcher fault backoff promptly instead of waiting through retry sleeps.

## Host-Facing Observability

- `kernel_get_state(...)` reports `CATCHING_UP / READY / UNAVAILABLE / REBUILDING`.
- Diagnostics export mirrors healthy `CATCHING_UP` without inventing faults.
- Diagnostics export mirrors live watcher faults while degraded.
- Diagnostics fault history retains recent watcher failures after recovery.
- Diagnostics export the latest rename/move continuity fallback reason and timestamp.

## Phase Gate Expectation

Before accepting watcher lifecycle changes:

1. `kernel_api_tests` must pass.
2. `kernel_phase_gate` must pass.
3. No watcher lifecycle regression may require manual interpretation of runtime state.
4. Any new watcher fault or shutdown rule must be reflected here.
