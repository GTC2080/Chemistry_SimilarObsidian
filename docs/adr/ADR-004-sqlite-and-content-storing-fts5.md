> Reason: This file freezes the local storage and full-text indexing direction before implementation expands.

# ADR-004: SQLite with Content-Storing FTS5

## Status
Accepted

## Context
Phase 1 needs a rebuildable local state store and a simple full-text engine, while explicitly avoiding a custom full-text index.

## Decision
The local state store is SQLite and note full-text search uses a content-storing FTS5 table with `rowid = note_id`.

## Consequences
- Identity mapping is simple.
- Indexed text is duplicated in the FTS table.

## Rejected Alternatives
- External-content FTS5
- Contentless FTS5
- Custom full-text index
