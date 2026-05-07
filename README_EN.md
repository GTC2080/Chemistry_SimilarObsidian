<p align="center">
  <a href="README.md">简体中文</a> | English
</p>

<p align="center">
  <img src="apps/tauri/.logo/Logo.png" width="116" alt="Nexus · Scientist Obsidian Logo" />
</p>

<h1 align="center">Nexus · Scientist Obsidian</h1>

<p align="center">
  <strong>A local-first intelligent knowledge workspace for scientific study and research.</strong>
</p>

<p align="center">
  Markdown notes, papers, chemistry tools, knowledge graphs, AI workflows, and study feedback in one desktop app.
</p>

<p align="center">
  <img src="https://img.shields.io/badge/Tauri-2.x-24C8DB?logo=tauri&logoColor=white" alt="Tauri 2" />
  <img src="https://img.shields.io/badge/React-19-61DAFB?logo=react&logoColor=111111" alt="React 19" />
  <img src="https://img.shields.io/badge/C%2B%2B-20-00599C?logo=cplusplus&logoColor=white" alt="C++20" />
  <img src="https://img.shields.io/badge/SQLite-local_truth-044A64?logo=sqlite&logoColor=white" alt="SQLite" />
</p>

---

## What This Project Is

**Nexus · Scientist Obsidian** is a desktop knowledge management app for scientific learning, research, and long-lived personal knowledge work.

It is not just a note editor, and it is not just an AI chat wrapper. It brings your vault, papers, chemistry tools, semantic search, graph navigation, and AI context into one workflow so that notes, references, calculations, and questions stay connected.

The project is local-first by default. Vault files, indexes, and core state are stored on your machine first. AI features can connect to external models, but the app is designed around your local knowledge base rather than a cloud-first workspace.

## Who It Is For

- Students, researchers, and technical users who maintain long-lived Markdown knowledge bases.
- People who read papers, organize PDFs, and build links between notes and references.
- Chemistry-heavy study or research workflows that need molecular editing, previews, spectroscopy, stoichiometry, crystal tools, and symmetry analysis.
- Users who want semantic search, RAG context, and AI chat grounded in their own vault.
- Obsidian-style local knowledge users who want deeper scientific tooling and AI integration in a desktop app.

## Core Capabilities

### Knowledge Base

- Markdown notes, file tree, tag tree, wiki-links, and backlinks.
- LaTeX math, task lists, tables, code blocks, and a rich editing experience.
- Global graph exploration for notes, tags, and link structure.

### Papers And Reference Reading

- PDF reading, highlights, ink strokes, annotations, metadata, and anchor persistence.
- Notes and references can live in the same vault, reducing context switching between tools.

### Science And Chemistry Tools

- Ketcher molecular editing and molecular previews.
- Spectroscopy parsing, stoichiometry, kinetics, and crystal utilities.
- Point-group / symmetry analysis and retrosynthesis-facing surfaces.

### AI Knowledge Flow

- Semantic search, related notes, embedding cache, and RAG context building.
- Chat workflows can use the current vault, current note, and retrieval results as context.
- AI is part of the knowledge workflow, not a replacement for the local knowledge base.

### Study Feedback

- Study sessions, learning statistics, and heatmaps.
- product compute / TRUTH_SYSTEM feedback surfaces for observing the state of the vault and study workflow.

## Architecture

Nexus · Scientist Obsidian uses React + Tauri for the desktop experience, with long-lived rules and durable truth held behind a C++20 sealed kernel.

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

The split is intentional: the desktop host owns windows, platform integration, and interaction orchestration; the kernel owns rules that affect durable state, query behavior, compute output, and cross-host compatibility.

## Repository Layout

```text
.
|-- README.md                 # Chinese README
|-- README_EN.md              # English README
|-- apps/
|   `-- tauri/                # current desktop app: React + Tauri + Rust bridge
|-- docs/                     # repo-level integration and structure notes
`-- kernel/                   # C++20 sealed kernel, tests, docs, benchmarks
    |-- include/kernel/       # public C ABI
    |-- src/internal/         # kernel internal headers
    |-- src/impl/core/        # public-surface implementations grouped by domain
    |-- tests/                # API / parser / search / watcher tests
    |-- benchmarks/           # startup / IO / rebuild / query gates
    `-- docs/                 # ADRs, contracts, regression matrices, governance
```

`kernel/src/impl/core/` is grouped by public surface, including AI, attachments, chemistry, crystal, diagnostics, domain, notes, PDF, product, runtime, search, study, and symmetry modules.

## Current State

- **Project name**: Nexus · Scientist Obsidian
- **Target repository name**: `Nexus-Scientist-Obsidian`
- **Current host**: Tauri desktop app
- **Core kernel**: C++20 sealed kernel
- **Kernel milestone**: `stage-phase2-track5-gated`
- **Phase 1**: host-stable kernel baseline complete
- **Phase 2 Track 1-5**: complete and gated

## Quick Start

### Requirements

- Windows
- Visual Studio 2022 Build Tools with C++ workload
- CMake 3.21+
- Node.js 18+
- Rust 1.77+ and Tauri 2 prerequisites

On this workstation, the kernel build environment is normally entered through:

```powershell
E:\Dev\bin\kernel-dev-x64.cmd
```

### Kernel

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

### Desktop App

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

## Lineage

Nexus · Scientist Obsidian continues the product direction of [GTC2080/Nexus](https://github.com/GTC2080/Nexus): local-first knowledge management, AI assistance, and scientific learning.

The original Rust-backed Nexus is no longer the primary update line. New feature development, architecture work, and kernel evolution are now focused on this repository.

```text
Nexus
  local-first AI knowledge workspace
      |
      v
Nexus · Scientist Obsidian
  science-focused local knowledge workspace
```
