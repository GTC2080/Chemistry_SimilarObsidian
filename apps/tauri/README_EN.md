<p align="center">
  English | <a href="README.md">简体中文</a>
</p>

<p align="center">
  <img src=".logo/Logo.png" width="120" alt="Nexus Logo" />
</p>

<h1 align="center">Nexus</h1>

<p align="center">
  A local-first intelligent knowledge management tool with an Obsidian-like experience and built-in AI capabilities.
</p>

<p align="center">
  <img src="https://img.shields.io/badge/Tauri-2.x-blue?logo=tauri" alt="Tauri 2" />
  <img src="https://img.shields.io/badge/React-19-61dafb?logo=react" alt="React 19" />
  <img src="https://img.shields.io/badge/Rust-2021-orange?logo=rust" alt="Rust" />
  <img src="https://img.shields.io/badge/license-MIT-green" alt="MIT License" />
</p>

---

## Highlights

- **Local-first knowledge base** — Markdown + SQLite + filesystem storage with `[[wikilinks]]`, `#tags`, LaTeX math, and image/PDF preview
- **AI-assisted workflows** — semantic search, related-note resonance, and RAG chat on top of OpenAI-compatible APIs
- **Chemistry-native workspace** — Ketcher 2D editor, 3D molecular viewer, symmetry analysis, crystal lattice tools, spectroscopy, and polymer kinetics
- **Publishing pipeline** — `.paper` workspace for drag-and-drop assembly plus Pandoc + XeLaTeX PDF generation
- **Relationship-driven navigation** — file tree, tag tree, knowledge graph, and study timeline work together for writing and review
- **Desktop performance focus** — Tauri hosts the shell while the C++ sealed kernel owns core vault/query/embedding retrieval surfaces; recent work includes incremental vault watching, leaner PDF rendering, and smoother panel resizing
- **Polished app shell** — onboarding, themes, bilingual UI, resizable layout, and the TRUTH_SYSTEM dashboard are already integrated

## Current Status

- **Current target release**: `v1.0.5`
- **Release focus**: chemistry-heavy today, while the Markdown, search, graph, and AI layers remain general-purpose
- **Data policy**: local-first by default, with vault content and SQLite data kept on the user's machine
- **Optimization direction**: large-vault responsiveness, low-blocking I/O, fast interaction feedback, and stable semantic retrieval
- **Backend boundary**: core note IO, search, tags, backlinks, graph, file tree, and AI embedding cache retrieval are owned by `kernel/`; Tauri Rust keeps command registration, bridge wiring, platform glue, and thin orchestration

## Recent Updates

- **v1.0.6-dev** — the legacy embedding DB has moved into the C++ kernel: `ai_embedding_cache` now owns metadata, timestamps, vectors, clear/delete, and semantic top-k retrieval; Rust no longer keeps a local embedding SQL layer or local cosine-similarity cache
- **v1.0.5** — the current unreleased target combines the 15-item performance pass and the crystal-lattice feature set; this includes save-queue safety, incremental watcher flow, PDF path rendering, zero-rerender resize, `.cif` tri-view support, supercell generation, and Miller-plane slicing
- **v1.0.4** — moved multiple hot-path computations from the frontend into Rust to improve startup, switching, and heavy-view responsiveness

## Tech Stack

| Layer | Technology |
|-------|-----------|
| Framework | Tauri 2 |
| Frontend | React 19 + TypeScript + Tailwind CSS 4 |
| Editor | TipTap 3 + KaTeX + 3Dmol.js + Ketcher |
| Backend | Tauri Rust shell + C++ sealed kernel + SQLite |
| AI | OpenAI-compatible API (Chat + Embedding) |
| Build | Vite 6 |

## Getting Started

### Prerequisites

- [Node.js](https://nodejs.org/) >= 18
- [Rust](https://rustup.rs/) >= 1.77
- [Tauri 2 CLI prerequisites](https://v2.tauri.app/start/prerequisites/)

### Install & Run

```bash
# Clone the repository
git clone https://github.com/GTC2080/Nexus.git
cd Nexus

# Install frontend dependencies
npm install

# Start in development mode (compiles Rust + starts frontend)
npx tauri dev

# Build for production
npx tauri build
```

## AI Configuration

Open Settings (⌘,) from the bottom-left corner of the app and fill in:

- **Chat Model** — API Key, Base URL, model name (default: `gpt-4o-mini`)
- **Embedding Model** — Optional; leave empty to reuse Chat config (default: `text-embedding-3-small`)

Any OpenAI-compatible API endpoint is supported.

## Spectroscopy Data Support

Open instrument-exported spectral data files directly in the app:

| Format | Description |
|--------|-------------|
| `.csv` | Comma/tab-separated spectral data (UTF-8 and UTF-16 LE supported) |
| `.jdx` | JCAMP-DX standard format (universal chemistry interchange format) |

- Automatically detects multi-column data (e.g. multiple scans) and renders each as a separate trace
- NMR data is auto-detected with reversed x-axis (chemical shift from high to low field)
- Spectral files are excluded from database content indexing and embedding vectorization to avoid token waste on massive float arrays

## 3D Molecular Structure Support

The app is currently chemistry-focused, and you can directly open the following 3D structure files:

| Format | Description |
|--------|-------------|
| `.pdb` | Protein Data Bank — protein and small-molecule structures |
| `.xyz` | XYZ coordinate format (common in computational chemistry) |
| `.cif` | Crystallographic Information File |

- Small molecules (≤ 500 atoms) default to ball+stick rendering; proteins automatically switch to cartoon mode
- Dark-fusion background with Jmol standard atom coloring
- `.cif` files provide a three-view mode: "Structure / Symmetry / Lattice" with supercell expansion (1-5×) and Miller index slicer; non-CIF molecular files retain the "Structure / Symmetry" dual view
- Symmetry view shows point-group HUD, rotation axes, and mirror planes (individually togglable)
- Symmetry computation runs in Rust and supports PDB / XYZ / CIF; CIF cell parameters are accepted in both "same-line value" and "next-line value" styles
- Molecular files are excluded from database content indexing and embedding vectorization to prevent massive coordinate data from polluting semantic search

## Polymer Kinetics Sandbox

In chemistry mode, click `POLYMER KINETICS` in the Markdown editor to open a full-screen dark simulator:

- Left panel controls: `[M]0`, `[I]0`, `[CTA]0`, `kd`, `kp`, `kt`, `ktr`, `timeMax`, `steps`
- Frontend IPC is debounced (`150ms`) to avoid request congestion during slider drags
- Rust backend tracks radical/monomer states and 0/1/2 moments, returning `time / conversion / Mn / PDI`
- Two live Plotly charts:
  - `Conversion vs Time`
  - `Mn / PDI vs Conversion` (`PDI` on right y-axis)
- Initial divide-by-zero is guarded: before chain formation, `Mn = 0` and `PDI = 1.0`

## Project Structure

```
src/                    # React frontend
├── assets/             # Static assets (Logo / icons)
├── components/         # UI components
│   ├── app/            # Workspace shell, runtime, viewport, LaunchSplash, and modals
│   ├── ai/             # AI assistant sub-components (ChatBubble / AIContextPanel)
│   ├── KineticsSimulator.tsx  # Polymer kinetics sandbox (chemistry mode)
│   ├── CrystalViewer3D.tsx   # Crystal lattice 3D renderer (supercell + Miller plane slicer, chemistry mode)
│   ├── AIAssistantSidebar.tsx # AI assistant sidebar (thin orchestrator, logic in hooks)
│   ├── MarkdownEditor.tsx     # Main Markdown editor (thin orchestrator, extensions in hook)
│   ├── onboarding/     # First-run onboarding wizard
│   ├── study-timeline/ # Auto study timeline panel (heatmap/stats/daily records)
│   ├── chem-editor/    # Ketcher chemical editor components
│   ├── editor/         # Editor-facing UI components
│   ├── global-graph/   # Global knowledge graph view
│   ├── markdown-editor/ # Markdown editor menus, BubbleMenuBar, context actions, and helpers
│   ├── media-viewer/   # Image/PDF/spectroscopy preview components
│   ├── publish-studio/ # Assembly workspace for paper and note publishing flows
│   ├── search/         # Search results and semantic retrieval UI
│   ├── settings/       # Settings panels (split into 5 independent panels + shared components)
│   └── sidebar/        # File tree, tag tree, and side tools
├── i18n/               # Internationalization (i18n)
│   ├── zh-CN.ts        # Chinese translation dictionary
│   ├── en.ts           # English translation dictionary
│   ├── context.tsx     # LanguageProvider / useT / useLanguage
│   └── types.ts        # Language type definitions
├── editor/             # TipTap editor extensions
│   └── extensions/     # WikiLink / Tag / Math / ChemDraw
├── hooks/              # React hooks
│   ├── useVaultSession.ts           # Session orchestrator composed from indexing, content, persistence, and preview hooks
│   ├── useVaultIndex.ts             # Vault scanning, reindexing, and active-note reconciliation
│   ├── useActiveNoteContent.ts      # Active note content, preview loading, and discipline-specific state
│   ├── useAIChatStream.ts           # AI chat streaming logic (message history / streaming render / IPC channel)
│   ├── useMarkdownEditorExtensions.ts # TipTap extension configuration (memoized)
│   ├── useSettingsModal.ts          # Settings modal state management and operation logic
│   ├── useFileTreeDragDrop.ts       # File tree drag-and-drop logic
│   ├── useInlineRename.ts           # Inline rename logic
│   ├── useSidebarTags.ts            # Tag panel loading and filtering logic
│   ├── useSemanticResonance.ts      # Semantic resonance context building, caching, and adaptive debounce
│   ├── useNotePersistence.ts        # Save dedupe, queued writes, and explicit flush control
│   ├── useStudyTracker.ts           # Auto study timing hook (activity detection + Tauri IPC)
│   ├── useRuntimeSettings.ts        # Settings load/save logic
│   ├── useTruthSystem.ts            # TRUTH_SYSTEM dashboard state and interactions
│   └── ...                          # Other performance and interaction hooks
├── models/             # Frontend domain models
├── types/              # Split type definitions
├── *.test.ts           # Vitest unit-test entry points (types, settings, semantic resonance, etc.)
├── utils/              # Utilities (parsing/formatting/shared algorithms)
└── types.ts            # Legacy-compatible type entry

src-tauri/src/          # Rust backend
├── commands/           # Modular Tauri command handlers
│   ├── cmd_vault.rs    # Vault lifecycle and file operation commands
│   ├── cmd_tree.rs     # File tree/tag tree build and query commands
│   ├── cmd_search.rs   # Search / FTS / semantic retrieval commands
│   ├── cmd_ai.rs       # AI chat and reasoning commands
│   ├── cmd_study.rs    # Study timeline recording and statistics commands
│   ├── cmd_compute.rs  # TRUTH diff and compute commands
│   ├── cmd_media.rs    # Media and spectroscopy parsing commands
│   ├── cmd_symmetry.rs # Molecular symmetry analysis commands (point group / axes / planes)
│   └── cmd_crystal.rs  # Crystal lattice parsing and Miller plane commands (CIF → supercell → slicing plane)
├── commands.rs         # Command registration entry
├── crystal/            # Crystal lattice command DTOs
│   ├── mod.rs          # DTO re-export; full-result compute runs through sealed kernel bridge
│   └── types.rs        # Lattice data protocol (LatticeData / UnitCellBox / AtomNode / MillerPlaneData)
├── kinetics.rs         # Polymer kinetics kernel bridge
├── db.rs               # SQLite database management
├── db/                 # DB submodules
│   ├── schema.rs       # Study schema and migrations
│   ├── study.rs        # Study module entry (submodules: session / stats / truth)
│   ├── study/          # Study module subdir
│   │   ├── session.rs  # Session CRUD (start / tick / end)
│   │   ├── stats.rs    # Statistics aggregation queries (heatmap / daily / ranking)
│   │   └── truth.rs    # TruthState experience-level derivation
│   └── common.rs       # Shared DB utilities
├── shared/             # Shared helpers across command and service modules
├── services/           # Domain service layer
├── symmetry/           # Symmetry command DTOs; compute runs through sealed kernel bridge
├── watcher/            # Incremental file system watcher
│   ├── mod.rs          # WatcherState lifecycle (start/stop)
│   ├── filter.rs       # Path filtering (hidden files / extension whitelist / ignored folders)
│   └── handler.rs      # Event callback (classify / dedup / IPC emit)
├── ai/                 # AI module (split by responsibility)
│   ├── mod.rs          # AiConfig definition and unified re-exports
│   ├── embedding.rs    # Embedding requests, LRU cache, and concurrency control
│   └── chat.rs         # Streaming RAG chat and Ponder node generation
├── error.rs            # Typed error handling (AppError / AppResult)
├── models.rs           # Data models
└── lib.rs              # App entry point
```

## Architecture Evolution (Recent)

- **Kernel embedding cache migration (v1.0.6-dev)**: semantic search, related notes, and RAG candidate retrieval now call kernel top-k over `ai_embedding_cache`; Rust only schedules embedding requests and maps command DTOs.
- **Full performance optimization pass (target release v1.0.5)**: the current unreleased target combines 15 items across P0-P3 and the crystal-engine work. That includes save queue safety, incremental watcher flow, PDF fallback/path rendering improvements, zero-rerender resize, graph/file-tree FNV-1a fingerprints, and perf baseline tooling.
- **Extreme performance optimization (v1.0.4)**:
  - Rust `scan_vault` restructured from per-file locking to batch timestamp pre-read + single-transaction writes, reducing Mutex overhead by ~99%
  - `rebuild_vector_index` changed from sequential to 4-way concurrent streaming with batched DB writes
  - High-frequency frontend components fully `React.memo`-ized; graph callbacks extracted to stable references
  - Global CSS `transition-all` replaced with precise property transitions across 10+ components
  - Launch splash screen replaces blank loading page with branded Logo + breathing glow + progress bar animation
- **Component splitting & hook extraction (v1.0.4)**: six 300-510 line "god components" split by single-responsibility principle, producing 11 new files (6 hooks + 5 sub-components) where each file does one thing
- **Lean app container**: `App.tsx` has been refactored into an orchestration layer instead of mixing state, business logic, and rendering
- **Frontend responsibility split**: `components/app/` now separates workspace shell, runtime, editor viewport, and `ActiveNoteContent` to keep render dispatch manageable
- **Layered session logic**: `useVaultSession` now orchestrates dedicated hooks for indexing, active content, binary previews, and queued persistence
- **Safer persistence flow**: added save fingerprint dedupe, queued disk writes, and explicit flush steps to reduce redundant writes and note-switch risk
- **Semantic + AI performance work**: semantic resonance now uses context extraction, caching, and adaptive debounce; the AI sidebar separates historical and streaming message rendering
- **Testing baseline added**: introduced `Vitest + jsdom` to cover types, settings, and semantic recommendation logic with foundational unit tests
- **Cross-view consistency**: theme-token system now covers both light/dark modes consistently across Settings, Sidebar, and TRUTH_SYSTEM dashboard
- **Modular Rust commands**: command handlers moved from monolithic `commands.rs` into `commands/` submodules for easier maintenance and testing
- **AI module split**: `ai.rs` refactored into `ai/` submodules (embedding / chat / similarity) for better maintainability
- **Shared + service layers**: `shared/` and `services/` host reusable helpers and domain logic to reduce duplication in command handlers
- **Multi-language system**: lightweight React Context–based i18n in `i18n/`; `useT()` hook drives translation switching across the component tree with parameter interpolation
- **Typed error handling**: Rust-side `AppError` enum via `thiserror` replaces all `Result<T, String>`, with serializable error types forwarded to the frontend
- **React Compiler**: integrated babel-plugin-react-compiler for automatic memoization, eliminating manual `useMemo`/`useCallback` maintenance
- **Global Error Boundary**: `ErrorBoundary` wraps all Suspense zones so a single module crash cannot take down the entire app
- **Expanded test coverage**: test suite grew from 13 to 40 cases, covering core hooks (persistence, debounce, file actions) and ErrorBoundary
- **Chemical editor**: replaced @xyflow/react generic canvas with Ketcher professional molecule editor (.mol format), CSS penetration for absolute-minimalism dark theme, /chemdraw slash command for inline molecule insertion in Markdown

## License

[MIT](LICENSE)
