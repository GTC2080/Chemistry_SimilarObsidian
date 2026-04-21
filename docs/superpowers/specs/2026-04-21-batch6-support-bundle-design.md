# Batch 6 Support Bundle / Rebuild Observability Design

## Goal

Close Batch 6 by turning the current diagnostics snapshot into a more useful support bundle, extending rebuild observability with a stable duration field, and freezing one repository-local release checklist that names the required gates and smoke checks.

## Scope

This design covers:

- `P2-1` diagnostics support bundle
- `P2-2` rebuild observability
- `P2-3` crash/recovery/support release checklist

This design does not introduce:

- a new public C ABI
- live progress reporting
- cancellation
- a persistent logging backend
- a CI platform redesign

## Constraints

- Diagnostics must remain a single synchronous `kernel_export_diagnostics(...)` JSON snapshot.
- The repo currently has a `NullLogger`; there is no real persisted log sink to export or reference.
- Release verification already flows through `kernel_phase_gate`; Batch 6 should formalize it instead of inventing a parallel gate.
- Rebuild observability should reuse the existing runtime timestamps and task lifecycle.

## Design

### 1. Diagnostics Support Bundle

Keep the current top-level diagnostics fields and add two frozen pieces:

- `recent_events`
  - bounded append-only recent event history in stable order
  - newest event remains last
  - each event exports:
    - `kind`
    - `detail`
    - `code`
    - `at_ns`
- `logger_backend`
  - a stable string describing whether logs are available
  - for the current implementation this should explicitly report `null_logger`

The purpose is to answer both:

- “is the runtime currently healthy?”
- “what happened recently?”

### 2. Event Model

Recent events should stay deliberately small and only record already-frozen runtime milestones:

- startup recovery outcome
- live fault set
- continuity fallback
- rebuild started
- rebuild succeeded
- rebuild failed
- watcher recovered to healthy ready state after a prior live fault

Do not add generic free-form logging here.
This is a bounded support timeline, not a debug trace.

### 3. Rebuild Observability

Extend the rebuild snapshot with:

- `last_rebuild_duration_ms`

Semantics:

- duration is derived from the rebuild task’s start and completion timestamps
- the field reports the most recent completed rebuild duration
- before any completed rebuild it is `0`
- success and failure both update the duration

Existing rebuild fields remain authoritative:

- `last_rebuild_result`
- `last_rebuild_at_ns`
- `last_rebuild_result_code`
- `rebuild_in_flight`
- generation fields

### 4. Release Checklist

Create one repository-local checklist that names:

- `kernel_phase_gate`
- benchmark baseline update rule
- recovery crash-matrix checklist
- watcher lifecycle checklist
- diagnostics smoke expectation

The checklist should not invent new automation.
It should document exact commands and explicit pass conditions for release readiness.

### 5. Non-Goals

- no multi-file support archive
- no log file generation
- no crash dump
- no telemetry
- no richer rebuild progress model

## Testing Strategy

Add diagnostics regressions for:

- recent events retained in stable order
- live fault clearing does not erase history
- rebuild success exports duration
- rebuild failure exports duration
- support bundle exports `logger_backend`

Then verify:

- `watcher_tests`
- `kernel_api_tests`
- `kernel_phase_gate`

## Acceptance

Batch 6 is complete when:

- diagnostics can answer both current health and recent history
- hosts can read the last rebuild result, timestamp, and duration from diagnostics
- release validation no longer depends on memory or chat history
- `kernel_phase_gate` remains green after the changes
