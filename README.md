<p align="center">
  <img src="apps/tauri/.logo/Logo.png" width="116" alt="Nexus Logo" />
</p>

<h1 align="center">Nexus Kernel · Chemistry_Obsidian</h1>

<p align="center">
  A C++ sealed-kernel migration of <a href="https://github.com/GTC2080/Nexus">Nexus</a>,
  built for local-first chemistry knowledge work.
</p>

<p align="center">
  <img src="https://img.shields.io/badge/Tauri-2.x-24C8DB?logo=tauri&logoColor=white" alt="Tauri 2" />
  <img src="https://img.shields.io/badge/React-19-61DAFB?logo=react&logoColor=111111" alt="React 19" />
  <img src="https://img.shields.io/badge/C%2B%2B-20-00599C?logo=cplusplus&logoColor=white" alt="C++20" />
  <img src="https://img.shields.io/badge/Rust-host_bridge-B7410E?logo=rust&logoColor=white" alt="Rust host bridge" />
  <img src="https://img.shields.io/badge/SQLite-local_truth-044A64?logo=sqlite&logoColor=white" alt="SQLite" />
</p>

---

## What This Project Is

This repository is the kernel-migration edition of **Nexus · 星枢**.

The original [GTC2080/Nexus](https://github.com/GTC2080/Nexus) is a local-first, AI-powered knowledge management app built with Tauri 2, React 19, and a Rust backend. It provides an Obsidian-like Markdown vault, wiki-links, tags, LaTeX math, semantic search, global graph exploration, PDF reading, AI workflows, and chemistry-oriented study tools.

This project keeps that product direction, but changes the center of gravity:

> The durable backend truth is being migrated out of the Rust host layer and into a sealed C++20 kernel.

Tauri and Rust remain important, but they are no longer the place where long-lived business rules should accumulate. The host owns desktop integration, command registration, process and network boundaries, and bridge code. The C++ kernel owns vault rules, indexes, durable state, query surfaces, compute behavior, recovery semantics, and cross-host contracts.

## Why The Migration Exists

The old architecture worked, but too many rules were living in places that were easy to duplicate:

- Rust commands held path normalization, default limits, query shaping, and data-mapping rules.
- AI, graph, study, PDF, chemistry, and product-compute behavior could drift across host modules.
- Desktop glue and durable domain logic were too close together.
- Future hosts would have had to reimplement the same rules instead of calling one kernel contract.

The migration turns Nexus into a more serious platform:

| Before | Now |
| --- | --- |
| Rust backend owns most application rules | C++ sealed kernel owns durable rules |
| Tauri commands mix orchestration and truth | Tauri Rust is a thin host bridge |
| Query and compute behavior can be duplicated | Kernel ABI defines stable behavior |
| Host-specific code is hard to reuse | Kernel surfaces are host-agnostic |
| Recovery and storage behavior are scattered | Kernel owns SQLite, recovery, and validation gates |

## Product Identity

Nexus Kernel is still a personal knowledge workspace, but its strongest shape is chemistry-heavy research and study:

- **Markdown vault**: notes, wiki-links, tags, backlinks, file tree, tag tree, and global graph.
- **AI knowledge workflow**: semantic search, related notes, RAG context building, chat and embedding pipelines.
- **PDF reading and annotation**: pdf.js rendering, highlights, ink strokes, metadata, anchors, and persistence.
- **Chemistry workspace**: Ketcher editing, molecular previews, spectroscopy parsing, stoichiometry, kinetics, crystal utilities, symmetry analysis, and retrosynthesis surfaces.
- **Study and truth system**: study sessions, heatmaps, learning stats, and product-compute feedback loops.
- **Local-first storage**: vault files and SQLite-backed kernel state stay on the machine by default.

## Architecture

```text
React 19 UI
    |
    | Tauri commands
    v
Rust host bridge
    |
    | sealed_kernel_bridge.*
    v
C++20 sealed kernel
    |
    +-- SQLite storage
    +-- vault and path rules
    +-- note/search/tag/graph query surfaces
    +-- PDF, chemistry, symmetry, crystal, AI, study, and product compute contracts
    +-- recovery and regression gates
```

### Repository Layout

```text
.
|-- README.md                 # the only project README
|-- apps/
|   `-- tauri/                # current desktop host: React + Tauri + Rust bridge
|-- docs/                     # repo-level integration and structure notes
`-- kernel/                   # sealed C++20 kernel, tests, docs, benchmarks
    |-- include/kernel/       # public C ABI
    |-- src/internal/         # internal kernel headers
    |-- src/impl/core/        # public-surface implementations grouped by domain
    |-- tests/                # API, parser, search, watcher tests
    |-- benchmarks/           # startup, IO, rebuild, query gates
    `-- docs/                 # ADRs, contracts, regression matrices, governance
```

### Kernel Core Layout

The kernel core implementation is grouped by public surface:

```text
kernel/src/impl/core/
|-- ai/
|-- attachments/
|-- chemistry/
|-- crystal/
|-- diagnostics/
|-- domain/
|-- notes/
|-- pdf/
|-- product/
|-- runtime/
|-- search/
|-- study/
`-- symmetry/
```

Domain engines that do not expose the C ABI directly remain beside core under folders such as `storage/`, `search/`, `pdf/`, `watcher/`, `chemistry/`, `crystal/`, and `symmetry/`.

## Host Boundary Rule

The host talks to the kernel through:

- `apps/tauri/src-tauri/native/sealed_kernel_bridge.h`
- `apps/tauri/src-tauri/native/sealed_kernel_bridge.cpp`
- `apps/tauri/src-tauri/src/sealed_kernel.rs`

If a rule affects durable state, query shape, compute output, recovery, cross-host compatibility, or long-term product semantics, it belongs in the kernel.

The host should not duplicate kernel-owned rules such as:

- vault path normalization
- file-extension derivation
- note catalog defaults
- graph, tag, backlink, and search construction
- AI embedding cache shape
- RAG context formatting
- PDF metadata and annotation state
- chemistry, crystal, symmetry, and product-compute behavior
- study session storage and aggregation

## Current State

- **Origin project**: [GTC2080/Nexus](https://github.com/GTC2080/Nexus)
- **Current host**: Tauri desktop app
- **Migration target**: Rust backend rules -> C++ sealed kernel
- **Kernel milestone**: `stage-phase2-track5-gated`
- **Phase 1**: host-stable kernel baseline complete
- **Phase 2 tracks 1-5**: complete and gated
- **Main development posture**: keep shrinking host-side duplicated rules and move durable behavior behind kernel contracts

## Requirements

- Windows
- Visual Studio 2022 Build Tools with the C++ workload
- CMake 3.21 or newer
- Node.js 18 or newer
- Rust 1.77 or newer with Tauri 2 prerequisites

On this workstation, the kernel build environment is normally entered through:

```powershell
E:\Dev\bin\kernel-dev-x64.cmd
```

## Kernel Commands

Run kernel commands from `kernel/`.

```powershell
cd kernel
cmake --preset dev
cmake --build --preset build-debug
ctest --preset test-debug
```

Focused targets:

```powershell
cmake --build --preset build-debug --target chem_kernel
cmake --build --preset build-debug --target kernel_api_tests
cmake --build --preset build-debug --target kernel_phase_gate
cmake --build --preset build-release
```

Kernel build output lives in `kernel/out/build`.

## App Commands

Run app commands from `apps/tauri/`.

```powershell
cd apps/tauri
npm install
npm run dev
npm run build
npm run test:run
npx tauri dev
npx tauri build
```

`npm run dev` starts the Vite frontend. `npx tauri dev` starts the full desktop app path.

## Documentation Map

This repository intentionally keeps only one README. Detailed documents live where they can stay precise:

- Repo structure: `docs/repo-structure.md`
- Integration plan: `docs/integration-plan.md`
- Kernel current status: `kernel/docs/status/kernel-phase1-status.md`
- Kernel architecture rules: `kernel/docs/architecture/multi-discipline-kernel-design-rules-v1.md`
- Kernel ADRs: `kernel/docs/adr/`
- Kernel contracts: `kernel/docs/contracts/`
- Kernel public surfaces: `kernel/docs/surfaces/`
- Kernel regression matrices: `kernel/docs/regression/`
- Kernel governance: `kernel/docs/governance/`

## Development Principles

- Keep the root README as the single project entrypoint.
- Put behavior contracts in `kernel/docs/contracts/`.
- Put regression expectations in `kernel/docs/regression/`.
- Put long-term architecture decisions in `kernel/docs/adr/`.
- Keep generated outputs, local IDE state, and build trees out of Git.
- Do not add new host-side copies of rules that belong in the sealed kernel.

## Lineage

Nexus Kernel is not a new product idea. It is the hardening path for Nexus:

```text
Nexus
  local-first AI knowledge workspace
      |
      v
Nexus Kernel / Chemistry_Obsidian
  the same product direction, rebuilt around a sealed C++ kernel
```

The goal is simple: keep the user-facing knowledge workspace expressive, while making the underlying truth model harder to drift, easier to test, and ready for more than one host.
