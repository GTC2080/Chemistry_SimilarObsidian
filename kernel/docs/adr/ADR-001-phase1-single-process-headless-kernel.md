> Reason: This file freezes the Phase 1 execution shape so the repository starts from an agreed kernel boundary.

# ADR-001: Phase 1 Single-Process Headless Kernel

## Status
Accepted

## Context
Phase 1 is focused on a stable local-first kernel, not on GUI, plugins, sync, or service orchestration.

## Decision
The Phase 1 kernel runs as a single-process headless library embedded inside a host process.

## Consequences
- Startup and deployment stay simple.
- Debugging stays local to one process.
- Process isolation is deferred.

## Rejected Alternatives
- Local service plus IPC
- Full application shell in Phase 1
