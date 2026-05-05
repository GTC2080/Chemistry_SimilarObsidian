<p align="center">
  <a href="README.md">简体中文</a> | English
</p>

<p align="center">
  <img src="apps/tauri/.logo/Logo.png" width="116" alt="Nexus Logo" />
</p>

<h1 align="center">Nexus Kernel · Chemistry_Obsidian</h1>

<p align="center">
  <strong>A C++ sealed-kernel migration of the original Rust-backed Nexus.</strong>
</p>

<p align="center">
  A local-first intelligent knowledge workspace for chemistry-heavy study and research.
</p>

<p align="center">
  <img src="https://img.shields.io/badge/Tauri-2.x-24C8DB?logo=tauri&logoColor=white" alt="Tauri 2" />
  <img src="https://img.shields.io/badge/React-19-61DAFB?logo=react&logoColor=111111" alt="React 19" />
  <img src="https://img.shields.io/badge/C%2B%2B-20-00599C?logo=cplusplus&logoColor=white" alt="C++20" />
  <img src="https://img.shields.io/badge/Rust-host_bridge-B7410E?logo=rust&logoColor=white" alt="Rust host bridge" />
  <img src="https://img.shields.io/badge/SQLite-local_truth-044A64?logo=sqlite&logoColor=white" alt="SQLite" />
</p>

---

## Project Identity

**Chemistry_Obsidian** is the C++ kernel migration edition of [GTC2080/Nexus](https://github.com/GTC2080/Nexus).

The original Nexus is a local-first AI knowledge management app built with **Tauri 2 + React 19 + Rust**. It provides an Obsidian-like Markdown vault, wiki-links, tags, LaTeX math, semantic search, global graph exploration, PDF reading, AI workflows, and chemistry-oriented research tools.

This repository keeps the same product direction, but changes the architectural center of gravity:

> Long-lived business rules, durable data truth, and compute logic are being migrated out of the Rust backend and into a stable C++20 sealed kernel.

Tauri and Rust still matter, but their role is narrowed to desktop hosting, command registration, platform adaptation, network requests, external process launching, and kernel bridging. Rules that need long-term stability, testability, and cross-host reuse belong in the C++ kernel.

## Why This Migration Exists

The previous Rust backend worked, but as the product grew, more rules accumulated in the host layer:

- Path normalization, default limits, query shaping, and DTO mapping rules were easy to duplicate.
- AI, graph, study, PDF, chemistry, and product-compute behavior could drift across modules.
- Desktop glue code and durable domain logic were too tightly coupled.
- Future hosts would have to reimplement rules instead of calling one stable kernel contract.

The goal is not to change languages for its own sake. The goal is to turn the backend into a platform kernel:

| Before | Now |
| --- | --- |
| Rust backend owns most business rules | C++ sealed kernel owns durable truth |
| Tauri commands mix orchestration and rule ownership | Tauri Rust becomes a thin host bridge |
| Query and compute behavior can be duplicated | Kernel ABI defines stable behavior |
| Host logic is hard to reuse | Kernel surfaces are host-agnostic |
| Storage, recovery, and validation are scattered | Kernel owns SQLite, recovery, and validation gates |

## Product Capabilities

Nexus Kernel is still a personal knowledge workspace, with its strongest shape in chemistry-heavy research and study:

- **Markdown vault**: notes, wiki-links, tags, backlinks, file tree, tag tree, and global graph.
- **AI knowledge workflow**: semantic search, related notes, RAG context building, chat, and embedding pipelines.
- **PDF reading and annotation**: pdf.js rendering, highlights, ink strokes, metadata, anchors, and annotation persistence.
- **Chemistry workspace**: Ketcher editing, molecular preview, spectroscopy parsing, stoichiometry, kinetics, crystal utilities, symmetry analysis, and retrosynthesis surfaces.
- **Study and feedback system**: study sessions, heatmaps, learning stats, and TRUTH_SYSTEM / product-compute feedback.
- **Local-first storage**: vault files and kernel SQLite state stay on the local machine by default.

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
    +-- vault / path rules
    +-- note / search / tag / graph query surfaces
    +-- PDF / chemistry / symmetry / crystal / AI / study / product compute contracts
    +-- recovery and regression gates
```

## Repository Layout

```text
.
|-- README.md                 # Chinese README
|-- README_EN.md              # English README
|-- apps/
|   `-- tauri/                # current desktop host: React + Tauri + Rust bridge
|-- docs/                     # repo-level integration and structure notes
`-- kernel/                   # C++20 sealed kernel, tests, docs, benchmarks
    |-- include/kernel/       # public C ABI
    |-- src/internal/         # kernel internal headers
    |-- src/impl/core/        # public-surface implementations grouped by domain
    |-- tests/                # API / parser / search / watcher tests
    |-- benchmarks/           # startup / IO / rebuild / query gates
    `-- docs/                 # ADRs, contracts, regression matrices, governance
```

## Kernel Core Layout

`kernel/src/impl/core/` is grouped by public surface:

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

Domain engines that do not expose the C ABI directly remain beside core, such as `storage/`, `search/`, `pdf/`, `watcher/`, `chemistry/`, `crystal/`, and `symmetry/`.

## Host / Kernel Boundary

The Tauri host calls the kernel through:

- `apps/tauri/src-tauri/native/sealed_kernel_bridge.h`
- `apps/tauri/src-tauri/native/sealed_kernel_bridge.cpp`
- `apps/tauri/src-tauri/src/sealed_kernel.rs`

The rule is simple:

> If a rule affects durable state, query shape, compute output, recovery, cross-host compatibility, or long-term product semantics, it belongs in the kernel.

The host should not duplicate kernel-owned rules such as:

- vault path normalization
- file-extension derivation
- note catalog default limit
- graph / tag / backlink / search construction
- AI embedding cache shape
- RAG context formatting
- PDF metadata / annotation state
- chemistry / crystal / symmetry / product compute behavior
- study session storage and aggregation

## Current State

- **Origin project**: [GTC2080/Nexus](https://github.com/GTC2080/Nexus)
- **Current host**: Tauri desktop app
- **Migration target**: Rust backend rules -> C++ sealed kernel
- **Kernel milestone**: `stage-phase2-track5-gated`
- **Phase 1**: host-stable kernel baseline complete
- **Phase 2 Track 1-5**: complete and gated
- **Development posture**: keep shrinking host-side duplicated rules and move durable behavior behind kernel contracts.

## Requirements

- Windows
- Visual Studio 2022 Build Tools with C++ workload
- CMake 3.21+
- Node.js 18+
- Rust 1.77+ and Tauri 2 prerequisites

On this workstation, the kernel build environment is normally entered through:

```powershell
E:\Dev\bin\kernel-dev-x64.cmd
```

## Kernel Commands

Run kernel commands from `kernel/`:

```powershell
cd kernel
cmake --preset dev
cmake --build --preset build-debug
ctest --preset test-debug
```

Common focused targets:

```powershell
cmake --build --preset build-debug --target chem_kernel
cmake --build --preset build-debug --target kernel_api_tests
cmake --build --preset build-debug --target kernel_phase_gate
cmake --build --preset build-release
```

Kernel build output lives in `kernel/out/build`.

## App Commands

Run app commands from `apps/tauri/`:

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

This repository keeps a Chinese README and an English README. Detailed documents live where they can stay precise:

- Repo structure: `docs/repo-structure.md`
- Integration plan: `docs/integration-plan.md`
- Kernel status: `kernel/docs/status/kernel-phase1-status.md`
- Kernel architecture rules: `kernel/docs/architecture/multi-discipline-kernel-design-rules-v1.md`
- Kernel ADR: `kernel/docs/adr/`
- Kernel contracts: `kernel/docs/contracts/`
- Kernel public surfaces: `kernel/docs/surfaces/`
- Kernel regression matrices: `kernel/docs/regression/`
- Kernel governance: `kernel/docs/governance/`

## Development Principles

- `README.md` is the Chinese entrypoint. `README_EN.md` is the English entrypoint.
- Behavior rules belong in `kernel/docs/contracts/`.
- Regression expectations belong in `kernel/docs/regression/`.
- Long-term architecture decisions belong in `kernel/docs/adr/`.
- Generated outputs, local IDE state, and build trees should stay out of Git.
- Do not add new host-side copies of rules that belong in the sealed kernel.

## Lineage

Nexus Kernel is not a new product idea. It is the architecture-hardening path for Nexus:

```text
Nexus
  local-first AI knowledge workspace
      |
      v
Nexus Kernel / Chemistry_Obsidian
  Same product direction, rebuilt around a C++ sealed kernel
```

The goal is simple: keep the user-facing workspace expressive and useful for research, while making the underlying truth model harder to drift, easier to test, and ready for reuse across hosts.
