> Reason: This file freezes the implementation language and the long-term external ABI boundary.

# ADR-002: C++20 with a Minimal C ABI

## Status
Accepted

## Context
The kernel needs modern internal systems-language support and a stable host-facing boundary that avoids C++ ABI leakage.

## Decision
The kernel is implemented in C++20 and exposed externally only through a minimal C ABI.

## Consequences
- Internal implementation can evolve without breaking hosts.
- Boundary conversion code is required.

## Rejected Alternatives
- Public C++ ABI
- Replacing C++20 with another implementation language in Phase 1
