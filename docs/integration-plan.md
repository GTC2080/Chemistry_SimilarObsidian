# Integration Plan

## Current Split

- `kernel/`
  - sealed C++ kernel and the authoritative product/storage/compute surfaces
- `apps/tauri/`
  - the only desktop host path: Tauri startup, commands, bridge calls, plugins,
    windows/dialogs, platform I/O, HTTP/process glue, and localized messages

`apps/electron/` is intentionally retired from the working tree. Do not add new
host code, renderer code, native bindings, or documentation under that path.

## Responsibility Split

- C++ kernel owns business truth, derived relationships, indexing/storage,
  product compute rules, chemistry/crystal/symmetry compute, PDF storage/query,
  file tree/catalog surfaces, graph/tag/search/backlink/tag-tree relations, and
  all kernel contract tests.
- Tauri Rust owns command registration, serde marshalling, sealed-kernel bridge
  calls, app lifecycle, platform-specific filesystem watcher wiring, temporary
  workspace/process execution, external HTTP calls, async task orchestration,
  and localized user-facing error text.
- Frontend code consumes Tauri commands and must not infer relationship,
  catalog, graph, tag, or file-tree truth from raw storage or duplicate logic.

## Current Integration Status

- `apps/tauri/src-tauri/src/db/` has been removed.
- Cargo manifests no longer depend on `rusqlite` or `sqlx`.
- search, backlinks, tags, tag tree, graph, note catalog, file tree, vault entry,
  PDF annotation/hash, AI embedding cache, study sessions, chemistry spectra,
  stoichiometry, kinetics, retrosynthesis, molecular preview, crystal, and
  symmetry surfaces route through the sealed kernel bridge.
- Paper compile planning, default template selection, and compile-log
  diagnostic summarization are kernel-owned; Rust only creates temp files and
  spawns Pandoc/XeLaTeX.
- PubChem query/result normalization is kernel-owned; Rust only performs HTTP
  and localized status/error mapping.
- `apps/electron/` and `apps/tauri/src-tauri/src/db/` are absent.

## Boundary Rule

- If a rule decides product behavior, derived data, storage shape, relationship
  truth, chemistry/math compute, limits, defaults, scoring, filtering, or
  normalization, it belongs in `kernel/`.
- If code exists only to talk to Tauri, the OS, a child process, the network, or
  the UI command layer, it may remain in `apps/tauri/src-tauri`.
- Every moved rule needs kernel API coverage plus the matching sealed bridge
  test when it crosses into Tauri.
- Every completed migration batch must leave stale Rust business code and stale
  Electron references out of the working tree.

## Canonical Validation

Use this minimum closure sequence for migration batches:

- `kernel/out/build/tests/Debug/kernel_api_tests.exe`
- `cargo test` from `apps/tauri/src-tauri`
- `npm test -- --run` from `apps/tauri`
- `npx tauri build --debug` from `apps/tauri`
