> Reason: This file freezes the consistency model that keeps recovery and future indexing predictable.

# ADR-003: Files Are Truth, Index Is Derived, Single Writer for Mutation

## Status
Accepted

## Context
The kernel must survive crashes and external edits without treating the index as a co-equal truth source.

## Decision
Vault files are the only truth source. Index data is derived. Mutations flow through a single writer lane.

## Consequences
- Recovery and rebuild remain straightforward.
- Write throughput is intentionally serialized.

## Rejected Alternatives
- Index as co-equal truth
- Multi-writer mutation model in Phase 1
