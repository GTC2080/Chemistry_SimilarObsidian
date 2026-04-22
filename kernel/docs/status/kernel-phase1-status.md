<!-- Reason: This file gives the repository one current stage snapshot so the sealed kernel line can be reviewed without replaying implementation history. -->

# Kernel Stage Status

Last updated: `2026-04-22`

## Purpose

This document is the current repository status snapshot.
It keeps the Phase 1 baseline visible, but it now tracks the sealed state of the repository on top of that baseline.

Its job is to answer four questions quickly:

1. What foundation is frozen?
2. Which capability lines are formal and gated?
3. What still sits outside the sealed node?
4. Is the repository currently in a taggable stage state?

## Current Read

Current milestone:

- `Phase 1 host-stable kernel baseline`: complete
- `Phase 2 Track 1: Search & Retrieval Expansion`: complete and gated
- `Phase 2 Track 2: Attachment Metadata & Content Surface`: complete and gated
- `Phase 2 Track 3: PDF Attachment & Source Reference Substrate`: complete and gated
- `Phase 2 Track 4: Domain Extension Substrate`: complete and gated
- `Phase 2 Track 5: Chemistry Capability Track v1`: complete and gated

Current maturity:

- the kernel foundation is stable
- the host-facing public surfaces are formalized
- diagnostics, regression matrices, benchmark baselines, and release checklist are all in-repo
- the repository is being managed as a stable staged node, not as an open-ended architecture exercise

## Frozen Foundation

The following foundation is closed and no longer treated as active design scope:

- runtime contract
- watcher lifecycle
- dirty shutdown / recovery matrix
- background rebuild lifecycle
- diagnostics support bundle
- benchmark gate wiring
- release checklist

Hosts can depend on the foundation without guessing:

- runtime state semantics
- rebuild state / fault alignment
- watcher fault / recovery behavior
- attachment truth rules
- PDF subtype boundary
- domain extension boundary
- chemistry spectra capability boundary

## Formal Capability Lines

### Phase 1 Baseline

Frozen and regression-backed:

- one-vault headless runtime
- note read / write
- SQLite-derived local state
- recovery journal and startup repair
- watcher-driven incremental refresh
- synchronous and background rebuild
- support-bundle diagnostics export

### Search

Formal and gated:

- search public surface
- snippet
- pagination
- filters
- ranking v1
- search contract and regression matrix

### Attachment

Formal and gated:

- attachment list / lookup / note refs / referrers public surface
- attachment metadata contract
- attachment lifecycle hardening
- attachment diagnostics and benchmark coverage

### PDF

Formal and gated:

- PDF metadata surface
- PDF source-anchor model
- note <-> PDF reference surface
- PDF diagnostics / rebuild / gate integration

### Domain Extension

Formal and gated:

- domain metadata namespace
- domain object subtype contract
- generic domain source-reference substrate
- domain diagnostics / rebuild / gate rules

### Chemistry Capability v1

Formal and gated:

- `chem.spectrum.*` metadata namespace
- chemistry spectra subtype contract
- note <-> chemistry spectrum source-reference surface
- chemistry diagnostics / rebuild / benchmark / gate integration

## Verification Snapshot

Latest full sealing verification: `2026-04-22`

Green checks:

- full Debug build path is healthy
- `ctest -C Debug --output-on-failure` passes
- `kernel_benchmark_gates` passes
- `kernel_phase_gate` passes

The current benchmark baselines, thresholds, and latest observed timings are frozen in:

- [benchmark-baselines.md](/E:/测试/Chemistry_Obsidian/kernel/docs/governance/benchmark-baselines.md)

The current release-ready command set is frozen in:

- [release-checklist.md](/E:/测试/Chemistry_Obsidian/kernel/docs/governance/release-checklist.md)

## Governance Surfaces Present

The repository now contains all required governance surfaces for the sealed node:

- capability contracts
- regression matrices
- benchmark baselines
- diagnostics export checks
- release checklist
- benchmark gates
- `kernel_phase_gate`

This is true for:

- search
- attachment
- PDF
- domain extension
- chemistry spectra

## Outside Current Sealed Node

The following remain outside the current sealed node:

- UI shells
- PDF reader behavior
- AI features
- plugin system
- cloud sync
- multi-vault runtime
- chemistry capability beyond spectra-first scope
- physics-specific capability tracks
- biology-specific capability tracks

These are not partial items inside the current seal.
They are outside it.

## Seal Read

Current judgment:

- the repository is in a stage-ready state
- current public surfaces, diagnostics, tests, benchmarks, and release checklist are aligned
- the current node is suitable for commit batching, tag creation, and later regression comparison
