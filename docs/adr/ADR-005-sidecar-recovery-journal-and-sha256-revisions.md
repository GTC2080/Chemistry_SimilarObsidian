> Reason: This file freezes the recovery truth source and the content revision rule before state persistence grows.

# ADR-005: Sidecar Recovery Journal and SHA-256 Content Revisions

## Status
Accepted

## Context
Recovery must not depend on SQLite, and save conflict detection must use a path-independent content identity.

## Decision
Recovery truth comes from a sidecar `recovery.journal`. Content revisions use `v1:sha256(raw_note_bytes)`.

## Consequences
- Recovery decisions survive database damage.
- Rename and move do not change content identity.

## Rejected Alternatives
- SQLite as the only recovery journal
- Path-based or mtime-based revision rules
