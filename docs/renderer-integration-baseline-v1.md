# Renderer Integration Baseline v1 — Backlog + Page Structure

> **Scope:** Renderer layer only. Host layer (`main/`, `preload/`, `kernel/`) is sealed.  
> **Premise:** `window.hostShell.*` is the only allowed bridge. All host calls return `{ok, data, error, request_id?}`.

---

## Horizontal Rules (Embedded Across All Batches)

### 1. Renderer Component / Page Tree Minimum

```
apps/electron/src/renderer/
  index.html              # entry shell
  app-shell.js            # bootstrap, routing, global state
  state/
    host-store.js         # single source of truth for host envelope data
    session-store.js      # session lifecycle + derived status
    runtime-store.js      # runtime summary polling
  components/
    layout/
      app-layout.js       # shell chrome + nav + content area
      nav-bar.js          # minimal top/side nav
    shared/
      state-surface.js    # loading / empty / unavailable / error
      host-error-card.js  # structured error display from envelope
      polling-indicator.js# subtle spinner for background polling
  pages/
    welcome-page.js       # vault not opened
    vault-page.js         # main workspace after vault open
    search-page.js        # dedicated search surface
    attachment-page.js    # attachment list + detail
    chemistry-page.js     # spectra list + detail
    diagnostics-page.js   # diagnostics + rebuild controls
  services/
    host-api-client.js    # thin wrapper over window.hostShell.*
    envelope-guard.js     # ok/data/error normalization + gap detection
```

#### Frozen Route / Page IDs

The following page identifiers are frozen for Baseline v1. All navigation, smoke tests, error fallbacks, and deep links must use these exact strings:

| Page ID | Purpose | Entry Condition |
|---|---|---|
| `welcome` | Vault not opened; session entry | `session.state === 'none'` |
| `vault` | Main workspace shell | `session.state === 'open'` |
| `search` | Search experience | Vault open |
| `attachments` | Attachment + PDF consumption | Vault open |
| `chemistry` | Chemistry spectra consumption | Vault open |
| `diagnostics` | Diagnostics + rebuild controls | Vault open (or global fallback) |

- Router implementation must treat these IDs as the canonical key set.
- No additional page IDs are introduced in Baseline v1.
- Smoke harness (`window.__rendererSmoke.navigateTo`) accepts only these six values.

### 2. Front-End State Shape ↔ Host Model Mapping

- Renderer state is **derived from host envelopes**, never authoritative.
- All host API results are stored as `envelope` objects (`{ok, data, error, request_id}`) in `host-store.js`.
- UI components read from store and render the envelope directly; transformation happens in dedicated mapper modules, not inline in components.
- Session state is **always** `session.getStatus()` result; renderer never caches session state across operations without re-querying.
- Runtime state is polled on a fixed interval (5s) when session is open; polling pauses when app is backgrounded.
- Store exposes `hostUnavailable` as a computed property: `!window.hostShell || last bootstrap call failed with ipcInvokeFailed/bridgeProtocolError`.

#### Store Single Source of Truth Rule (Frozen)

- `host-store.js` may **only cache host envelopes** (`{ok, data, error, request_id}`) as returned by `window.hostShell.*`.
- Derived UI flags (e.g., `isLoading`, `hasData`) are permitted as computed properties, but they must be recomputed from envelopes on every read.
- **Renderer must not invent a second business truth layer inside the store.** No local state may override, contradict, or guess host state.
- Examples of forbidden store content:
  - A cached `sessionReady` boolean that is set to `true` independently of `session.getStatus()`.
  - A local `indexState` that is inferred from timing rather than `runtime.getSummary()`.
  - Any optimistic update that pretends a host operation succeeded before the envelope confirms it.
- If the host envelope is stale, the UI must reflect the stale envelope until a new one arrives. The renderer does not "smooth over" stale data.

### 3. Loading / Empty / Unavailable / Error Component Rules

| State | Trigger | UI Rule |
|---|---|---|
| **Loading** | Async host call in flight, no prior data | `state-surface` renders spinner + context label (e.g., "Opening vault...") |
| **Empty** | Host returned `ok: true`, `data` is empty array / null / no records | `state-surface` renders empty icon + "No results" message; never show spinner |
| **Unavailable** | `window.hostShell` missing, or envelope code is `HOST_KERNEL_ADAPTER_UNAVAILABLE` / `HOST_IPC_INVOKE_FAILED` / `HOST_BRIDGE_PROTOCOL_ERROR` | Full-screen blocking surface with "Host unavailable" + retry action |
| **Error** | Host returned `ok: false` with specific error code | `host-error-card` renders code + message + details; if retryable, show retry button |

- These four states are **mutually exclusive** in presentation logic.
- A component receiving an envelope must classify into exactly one of the four before rendering content.

### 4. Renderer Boundary Rule

- Only `window.hostShell.*` is callable.
- Direct use of `require`, `process`, `fs`, `path`, Electron APIs, or Node APIs in renderer is forbidden.
- `host-api-client.js` is the single file allowed to touch `window.hostShell`; all other modules import from it.

### 5. Host Gap Reporting Rule

- If renderer needs a capability that `window.hostShell.*` does not expose, it is listed as an **Explicit Host Gap**.
- Renderer **must not** emulate, polyfill, or guess the missing capability.
- Host gaps are filed as blockers against the host baseline, not worked around in renderer.

### 6. Renderer Smoke Readiness Criteria

- All `window.hostShell.*` groups have at least one renderer page that exercises them through real UI interaction.
- Smoke path can validate renderer without `executeJavaScript` by inspecting DOM after simulated user actions.
- Error surfaces are reachable in smoke (host unavailable, session not open, degraded runtime).

### 7. UI Polish Boundary

- Baseline v1 uses system fonts, minimal CSS, no animation library.
- Color palette is restricted to neutral grays + one accent color for active states.
- All polish (themes, animations, rich typography) is deferred to post-baseline.

---

## [Batch 1] Renderer App Shell & Session Entry

### 目标
Replace the current placeholder `index.html` + `renderer.js` with a minimal app shell that can bootstrap, detect host availability, and route the user between "no vault" and "vault open" states.

### 为什么现在做
Without a shell, every downstream batch has nowhere to render. The shell establishes the renderer-side contract for state management, navigation, and error surfacing.

### Renderer / UI 要补什么

1. **`app-shell.js`** — Root module that:
   - Detects `window.hostShell` presence on load.
   - Calls `bootstrap.getInfo()` to confirm bridge health.
   - Sets up a minimal router (hash-based or state-driven) with two top-level modes:
     - `WELCOME` (no vault open)
     - `VAULT` (vault open)
   - Subscribes to `session.getStatus()` to trigger mode transitions.

2. **`welcome-page.js`** — Page shown when session state is `none` or `closing`:
   - Displays "Open Vault" button.
   - Provides path input (native `<input type="file" webkitdirectory>` is **not allowed** — renderer has no filesystem access; use a plain text input and let host validate the path, or defer folder picker to a future host gap).
   - Shows session open/close transitions with loading state.
   - Displays last session error if `session.getStatus()` returns `last_error`.

3. **`vault-page.js`** — Main workspace placeholder:
   - Header showing active vault path.
   - Minimal nav links to Search, Attachments, Chemistry, Diagnostics.
   - Content area for nested page rendering.

4. **`app-layout.js`** — Shell chrome:
   - Top bar: app title + runtime status indicator (green/yellow/red dot).
   - Nav area: links to top-level pages.
   - Content area: mounts current page.

5. **`host-store.js`** — Central store:
   - Holds last envelope for each API group.
   - Exposes `getHostAvailability()`, `getSessionState()`, `getRuntimeSummary()`.
   - Triggers re-render callbacks on update.

6. **`host-api-client.js`** — Thin wrapper:
   - Exports `callBootstrap()`, `callRuntime()`, `callSession()`, etc.
   - Wraps all calls in envelope normalization.
   - Logs host gaps if envelope shape is unexpected.

### 明确不做什么
- No multi-window support.
- No complex menu bar (use simple nav links).
- No visual theme system.
- No animations beyond CSS opacity transitions.
- No drag-and-drop file opening.

### 主要消费哪些现有 `window.hostShell.*` API
- `bootstrap.getInfo()` — confirm bridge, read versions, api_groups
- `runtime.getSummary()` — lifecycle_state, session, kernel_binding
- `session.getStatus()` — current session state + last_error
- `session.openVault(vaultPath)` — transition to open
- `session.closeVault()` — transition to closed

### 是否存在 host gap
- **Host Gap R-1A:** Folder picker dialog. Renderer cannot show a native folder picker without a new host API (`dialog.showOpenDialog` is main-process only). Workaround for baseline: user types path into text input; host validates.

### 若有 host gap，必须单列为“显式依赖项”
- `EXPLICIT-HOST-GAP-001`: `host.dialog.showOpenDialog({ properties: ['openDirectory'] })` exposed through preload so renderer can request vault path selection without direct Node access.

### 需要补哪些 renderer-side tests
- `app-shell.test.js`: bootstrap failure renders `Unavailable` surface.
- `welcome-page.test.js`: clicking "Open Vault" calls `session.openVault` with input value.
- `session-store.test.js`: store updates session state after `session.getStatus` envelope.

### 需要补哪些 integration / smoke checks
- Smoke opens app, asserts `welcome-page` is visible when no vault is open.
- Smoke opens vault via renderer UI, asserts `vault-page` becomes visible.
- Smoke closes vault, asserts return to `welcome-page`.

### 需要补哪些前端状态与错误处理规则
- If `bootstrap.getInfo()` fails on load → render `Unavailable` surface with retry.
- If `session.openVault()` fails → display `host-error-card` with error code + message; keep user on welcome page.
- If `session.getStatus()` returns `opening` for >30s → render degraded indicator (not blocked; user can still navigate).

### 验收标准
- [ ] App shell renders without errors in dev and packaged modes.
- [ ] Welcome page is shown when no vault is open.
- [ ] Vault page is shown after successful `session.openVault`.
- [ ] Closing vault returns to welcome page.
- [ ] Bootstrap failure renders full unavailable surface.
- [ ] All state transitions show correct `state-surface` variant.

---

## [Batch 2] Runtime / Session Status Consumption

### 目标
Surface runtime health and session state to the user through persistent UI chrome and dedicated status views.

### 为什么现在做
Users need to know whether the kernel is ready, catching up, rebuilding, or unavailable. This batch makes kernel state visible without requiring users to read JSON dumps.

### Renderer / UI 要补什么

1. **`runtime-status-badge.js`** — Persistent top-bar component:
   - Consumes `runtime.getSummary()` via polling.
   - Maps `kernel_runtime.index_state` to color dot:
     - `ready` → green
     - `catching_up` → yellow pulsing
     - `rebuilding` → amber
     - `unavailable` → red
   - Shows `kernel_binding.attached` state.
   - Shows `rebuild` in-flight indicator.

2. **`session-status-card.js`** — Detailed session panel:
   - Displays `session.state` (`none` / `opening` / `open` / `closing`).
   - Displays `active_vault_path`.
   - Displays `last_error` if present.
   - Displays `adapter_attached` boolean.

3. **`runtime-page.js`** (or tab within vault-page) — Diagnostic view:
   - Raw but formatted display of `runtime.getSummary()`.
   - Structured sections: lifecycle, kernel_runtime, rebuild, session, kernel_binding.

4. **`state-surface` extensions**:
   - `Degraded` variant: when index state is `catching_up` or `rebuilding`, show non-blocking banner instead of full loading overlay.
   - `SessionBusy` variant: when `session.state` is `opening` or `closing`, disable open/close buttons.

5. **Polling controller**:
   - Poll `runtime.getSummary()` every 5s while vault is open.
   - Pause polling when `document.visibilityState === 'hidden'`.
   - Resume on visibility return with immediate refresh.
   - Stop polling when session closes.

### 明确不做什么
- Do not rewrite host semantics (e.g., do not collapse `catching_up` + `ready` into one state).
- Do not invent a client-side state machine that mirrors the host state machine.
- Do not bypass host API to infer state from side effects.
- Do not expose raw C ABI details.

### 主要消费哪些现有 `window.hostShell.*` API
- `runtime.getSummary()` — full runtime snapshot
- `session.getStatus()` — session data

### 是否存在 host gap
- None. Runtime and session APIs already expose all required fields.

### 若有 host gap，必须单列为“显式依赖项”
- N/A

### 需要补哪些 renderer-side tests
- `runtime-status-badge.test.js`: maps index_state values to correct color classes.
- `session-status-card.test.js`: renders last_error when present; hides error section when null.
- `polling-controller.test.js`: starts/stops polling based on session state and visibility.

### 需要补哪些 integration / smoke checks
- Smoke opens vault, asserts badge shows green dot after index becomes `ready`.
- Smoke triggers rebuild via `rebuild.start`, asserts badge shows amber + rebuilding text.
- Smoke asserts runtime page renders all sections without undefined fields.

### 需要补哪些前端状态与错误处理规则
- If `runtime.getSummary()` fails once → keep last known data, show subtle retry indicator.
- If `runtime.getSummary()` fails 3 consecutive times → render `Unavailable` surface (host may have crashed).
- If `kernel_binding.attached === false` while session is open → render persistent warning banner: "Kernel adapter detached. Some features may be unavailable."
- If `index_state` transitions from `ready` → `rebuilding` → show non-blocking banner: "Index rebuilding. Search results may be incomplete."

### 验收标准
- [ ] Runtime badge is visible in app chrome at all times.
- [ ] Badge color correctly reflects `index_state`.
- [ ] Session card shows current vault path and state.
- [ ] Polling starts on vault open and stops on vault close.
- [ ] Three consecutive runtime poll failures render unavailable surface.
- [ ] Rebuild in-flight is visible without requiring user to open diagnostics page.

---

## [Batch 3] Search Experience Baseline

### 目标
Build a functional search page with input, results list, snippets, pagination, and basic filters.

### 为什么现在做
Search is the primary way users access vault content. It validates the read path from kernel → adapter → host → renderer end-to-end.

### Renderer / UI 要补什么

1. **`search-page.js`** — Top-level search surface:
   - Query input with submit on Enter.
   - Results list area.
   - Filter bar: `kind` (all/note/attachment), `tagFilter`, `pathPrefix`, `sortMode`.
   - Pagination controls: prev/next, offset/limit display.

2. **`search-result-list.js`** — List container:
   - Renders array of `search-result-item.js`.
   - Handles empty state via `state-surface`.
   - Handles pagination metadata (`total`, `has_more`).

3. **`search-result-item.js`** — Single result row:
   - Title (clickable, navigates to note).
   - Snippet / excerpt.
   - Rel path.
   - Tags (if present).
   - Backlinks count (if available in result shape).

4. **`search-view-model.js`** — Mapper from envelope to view model:
   - Transforms `search.query` envelope `data` into renderer-friendly shape.
   - Normalizes missing fields to safe defaults.
   - Logs host gap if expected fields are absent.

5. **`search-filter-bar.js`** — Filter controls:
   - `kind`: `<select>` with options `all`, `note`, `attachment`.
   - `tagFilter`: text input.
   - `pathPrefix`: text input.
   - `sortMode`: `<select>` with options matching host contract (`rel_path_asc`, etc.).
   - `includeDeleted`: checkbox.

### 明确不做什么
- No advanced search DSL.
- No AI search enhancement.
- No search recommendations.
- No client-side ranking or re-ranking.
- No fuzzy search beyond what host returns.

### 主要消费哪些现有 `window.hostShell.*` API
- `search.query({ query, limit, offset, kind, tagFilter, pathPrefix, includeDeleted, sortMode })`

### 是否存在 host gap
- **Host Gap R-3A:** Result item shape is not documented. Renderer needs stable fields. See "Search Result Item — Minimal Required Fields" below for the frozen split between required and optional.
- **Host Gap R-3B:** Pagination metadata shape. Renderer expects `total`, `has_more`, `offset`, `limit` in response. If absent, pagination UI cannot function.

#### Search Result Item — Minimal Required Fields (Frozen)

| Field | Requirement | Behavior if Missing |
|---|---|---|
| `title` | **Required** | Page cannot render result row; treat as host gap. |
| `rel_path` | **Required** | Page cannot render result row; treat as host gap. |
| `snippet` | **Required** | Page cannot render result row; treat as host gap. |
| `kind` | **Required** | Page cannot render result row; treat as host gap. |
| `tags` | Optional | Omit tags section if missing or empty. Do not block page. |
| `backlinks_count` | Optional | Omit backlinks display if missing. Do not block page. |

- The four required fields (`title`, `rel_path`, `snippet`, `kind`) form the **Search Baseline v1 contract**.
- Optional fields are rendered when present and silently skipped when absent.
- `search-view-model.js` must validate required fields on every envelope and flag a host gap if any are missing.

### 若有 host gap，必须单列为“显式依赖项”
- `EXPLICIT-HOST-GAP-002`: Documented and stable `search.query` response schema with `items[]`, `total`, `has_more`, `offset`, `limit`.
- `EXPLICIT-HOST-GAP-003`: Stable item fields: `title`, `rel_path`, `snippet`, `kind` (required); `tags`, `backlinks_count` (optional, non-blocking).

### 需要补哪些 renderer-side tests
- `search-page.test.js`: submitting query calls `search.query` with correct shape.
- `search-result-list.test.js`: renders `Empty` surface when `items` is empty array.
- `search-view-model.test.js`: maps envelope with missing fields to safe defaults and flags gap.
- `search-filter-bar.test.js`: changing sortMode triggers new query with updated params.

### 需要补哪些 integration / smoke checks
- Smoke opens vault, types "baseline" into search input, asserts results list renders non-empty.
- Smoke applies `kind: note` filter, asserts query params include `kind: "note"`.
- Smoke navigates to page 2, asserts `offset` increments by `limit`.

### 需要补哪些前端状态与错误处理规则
- If `search.query` returns `HOST_SESSION_NOT_OPEN` → render `Unavailable` surface with "Open a vault to search."
- If `search.query` returns `HOST_KERNEL_ADAPTER_UNAVAILABLE` → render `Unavailable` surface with retry.
- If `search.query` returns invalid argument → show inline error on filter bar, not global unavailable.
- Empty results → render `Empty` surface with "No notes or attachments match your search."

### 验收标准
- [ ] Search page is reachable from vault page nav.
- [ ] Query input triggers `search.query` on Enter.
- [ ] Results render title, snippet, rel path per item.
- [ ] Empty results show empty state, not error.
- [ ] Pagination prev/next work using offset/limit.
- [ ] Filters (kind, sortMode) are passed through to host API.
- [ ] Session-not-open error renders actionable unavailable surface.

---

## [Batch 4] Attachment & PDF Consumption Surface

### 目标
Build attachment list, attachment detail, and PDF metadata views. Surface note ↔ attachment ↔ PDF bidirectional refs.

### 为什么现在做
Attachments (including PDFs) are first-class vault content. Users need to browse, inspect metadata, and understand which notes reference which files.

### Renderer / UI 要补什么

1. **`attachment-page.js`** — Top-level attachment surface with two tabs:
   - **List tab**: all attachments.
   - **Detail tab**: single attachment view (opened by clicking list item or by direct navigation).

2. **`attachment-list.js`** — Grid or table of attachments:
   - Columns: rel_path, kind (inferred from extension), size (if host provides), modified date (if host provides).
   - Clicking row opens detail tab.
   - Pagination (limit/offset) if list is long.

3. **`attachment-detail.js`** — Single attachment view:
   - Full rel_path.
   - Metadata fields from `attachments.get`.
   - "Referrers" section: notes that link to this attachment (via `attachments.queryReferrers`).
   - If attachment is PDF → show PDF metadata subsection (via `pdf.getMetadata`).

4. **`pdf-metadata-card.js`** — PDF-specific metadata:
   - Fields: title, author, page_count, etc. (whatever host mapper provides).
   - "Note Source Refs" section: notes that cite this PDF (via `pdf.queryNoteSourceRefs`).

5. **`note-attachment-refs.js`** — Reusable component:
   - Given a `noteRelPath`, calls `attachments.queryNoteRefs` and renders attachment links.
   - Used on note detail pages (when note pages exist) and in attachment detail referrers.

6. **`attachment-view-model.js`** — Mapper:
   - Normalizes `attachments.list`, `attachments.get`, `attachments.queryNoteRefs`, `attachments.queryReferrers` envelopes.
   - Defines `AttachmentState` enum: `present`, `missing`, `stale`, `unresolved` based on host fields.

### 明确不做什么
- No PDF reader / page rendering.
- No text highlighting inside PDF.
- No annotation system.
- No file upload / drag-and-drop import.
- No file content preview beyond metadata.

### 主要消费哪些现有 `window.hostShell.*` API
- `attachments.list({ limit })`
- `attachments.get({ attachmentRelPath })`
- `attachments.queryNoteRefs({ noteRelPath, limit })`
- `attachments.queryReferrers({ attachmentRelPath, limit })`
- `pdf.getMetadata({ attachmentRelPath })`
- `pdf.queryNoteSourceRefs({ noteRelPath, limit })`
- `pdf.queryReferrers({ attachmentRelPath, limit })`

### 是否存在 host gap
- **Host Gap R-4A:** Attachment record shape is not fully documented. Renderer needs `rel_path`, `file_name`, `extension`, `size_bytes`, `modified_at`, `state` (present/missing/stale/unresolved). Missing fields degrade the UI.
- **Host Gap R-4B:** PDF metadata shape is not documented. Renderer needs `title`, `author`, `page_count`, `creation_date` at minimum.
- **Host Gap R-4C:** No host API to open external file. Renderer cannot launch system viewer for an attachment without a new host API.

### 若有 host gap，必须单列为“显式依赖项”
- `EXPLICIT-HOST-GAP-004`: Stable `attachments.get` response schema with `rel_path`, `file_name`, `extension`, `size_bytes`, `modified_at`, `state`.
- `EXPLICIT-HOST-GAP-005`: Stable `pdf.getMetadata` response schema with `title`, `author`, `page_count`, `creation_date`.
- `EXPLICIT-HOST-GAP-006`: `host.shell.openExternal(path)` or equivalent exposed through preload so renderer can request opening attachment in system default app.

### 需要补哪些 renderer-side tests
- `attachment-list.test.js`: renders rows from `attachments.list` envelope.
- `attachment-detail.test.js`: calls `attachments.get` + `attachments.queryReferrers` on mount.
- `pdf-metadata-card.test.js`: renders PDF metadata fields; shows "No PDF metadata" if envelope data is null.
- `attachment-view-model.test.js`: maps `state` field to `present/missing/stale/unresolved` correctly.

### 需要补哪些 integration / smoke checks
- Smoke opens vault, navigates to Attachments, asserts list renders sample.pdf and sample.jdx.
- Smoke clicks sample.pdf, asserts detail shows PDF metadata card.
- Smoke asserts referrers section includes `notes/example.md`.

### 需要补哪些前端状态与错误处理规则
- If `attachments.get` returns `HOST_SESSION_NOT_OPEN` → redirect to welcome page or show unavailable surface.
- If `attachments.get` returns record with `state: 'missing'` → render detail with warning banner: "Attachment file is missing on disk."
- If `attachments.get` returns record with `state: 'stale'` → render detail with info banner: "Attachment metadata may be out of date."
- If `pdf.getMetadata` fails (e.g., not a PDF) → hide PDF metadata card, not global error.

### 验收标准
- [ ] Attachment list page renders all attachments from vault.
- [ ] Attachment detail shows metadata + referrers.
- [ ] PDF attachments show PDF metadata subsection.
- [ ] Missing/stale states render appropriate banners.
- [ ] Note → attachment refs are renderable via reusable component.
- [ ] All attachment/PDF API errors render correct `state-surface` variant.

---

## [Batch 5] Chemistry Spectra Consumption Surface

### 目标
Build chemistry spectra list and detail views. Surface note ↔ spectrum bidirectional refs with whole-spectrum and x-range ref semantics.

### 为什么现在做
Chemistry is a core domain differentiator. Spectra metadata visibility validates the chemistry adapter path and gives users immediate value even without a spectrum viewer.

### Renderer / UI 要补什么

1. **`chemistry-page.js`** — Top-level chemistry surface with two tabs:
   - **List tab**: all spectra.
   - **Detail tab**: single spectrum view.

2. **`spectrum-list.js`** — Table of spectra:
   - Columns: title, data_type, x_units, y_units, source attachment rel_path.
   - Clicking row opens detail tab.

3. **`spectrum-detail.js`** — Single spectrum view:
   - Title, data_type, x_units, y_units, n_points.
   - Source attachment link (navigates to attachment detail).
   - "Note Refs" section: notes referencing this spectrum (via `chemistry.queryReferrers`).
   - Ref type indicator for each note ref:
     - `whole-spectrum` — generic link.
     - `x-range` — shows x_min/x_max inline.

4. **`note-chem-refs.js`** — Reusable component:
   - Given `noteRelPath`, calls `chemistry.queryNoteRefs`.
   - Renders spectrum links with ref type labels.

5. **`spectrum-view-model.js`** — Mapper:
   - Normalizes `chemistry.listSpectra`, `chemistry.getSpectrum`, `chemistry.queryNoteRefs`, `chemistry.queryReferrers` envelopes.
   - Defines `SpectrumState` enum: `present`, `missing`, `unresolved`, `unsupported`.
   - Maps `ref.kind` to `whole-spectrum` or `x-range` with optional `x_min`, `x_max`.

### 明确不做什么
- No spectrum chart viewer (no canvas plotting).
- No peak drag interaction.
- No AI spectrum interpretation.
- No chemistry beyond spectra-first scope (no reactions, no molecules, no stoichiometry).
- No file import for chemistry data.

### 主要消费哪些现有 `window.hostShell.*` API
- `chemistry.listSpectra({ limit })`
- `chemistry.getSpectrum({ attachmentRelPath })`
- `chemistry.queryMetadata({ attachmentRelPath, limit })`
- `chemistry.queryNoteRefs({ noteRelPath, limit })`
- `chemistry.queryReferrers({ attachmentRelPath, limit })`

### 是否存在 host gap
- **Host Gap R-5A:** Spectrum record shape is not documented. Renderer needs `attachment_rel_path`, `title`, `data_type`, `x_units`, `y_units`, `n_points`, `state`.
- **Host Gap R-5B:** Note ref item shape for chemistry is not documented. Renderer needs `note_rel_path`, `ref_kind` (`whole_spectrum` | `x_range`), `x_min`, `x_max`.
- **Host Gap R-5C:** `chemistry.queryMetadata` vs `chemistry.getSpectrum` overlap is unclear. Renderer needs clarity on which to call for detail view.

### 若有 host gap，必须单列为“显式依赖项”
- `EXPLICIT-HOST-GAP-007`: Stable `chemistry.getSpectrum` response schema with `attachment_rel_path`, `title`, `data_type`, `x_units`, `y_units`, `n_points`, `state`.
- `EXPLICIT-HOST-GAP-008`: Stable chemistry note-ref item schema with `note_rel_path`, `ref_kind`, `x_min`, `x_max`.
- `EXPLICIT-HOST-GAP-009`: Clarified semantics of `chemistry.queryMetadata` vs `chemistry.getSpectrum` — which is canonical for detail view.

### 需要补哪些 renderer-side tests
- `spectrum-list.test.js`: renders rows from `chemistry.listSpectra`.
- `spectrum-detail.test.js`: calls `chemistry.getSpectrum` + `chemistry.queryReferrers` on mount.
- `note-chem-refs.test.js`: renders x-range refs with min/max labels.
- `spectrum-view-model.test.js`: maps `unsupported` state to warning banner flag.

### 需要补哪些 integration / smoke checks
- Smoke opens vault, navigates to Chemistry, asserts spectrum list renders sample.jdx.
- Smoke clicks sample.jdx, asserts detail shows title, data_type, x_units, y_units.
- Smoke asserts note refs section shows `notes/example.md` with `whole-spectrum` or `x-range` label.

### 需要补哪些前端状态与错误处理规则
- If `chemistry.getSpectrum` returns `state: 'unsupported'` → render warning banner: "Spectrum format not supported for display."
- If `chemistry.getSpectrum` returns `state: 'missing'` → render warning banner: "Source file missing. Spectrum data unavailable."
- If chemistry APIs return `HOST_KERNEL_SURFACE_NOT_INTEGRATED` → render info banner: "Chemistry features are not available in this build." (graceful degradation).

### 验收标准
- [ ] Chemistry page is reachable from vault page nav.
- [ ] Spectrum list renders all spectra with title, data_type, x_units, y_units.
- [ ] Spectrum detail shows metadata + note referrers.
- [ ] X-range refs display x_min/x_max inline.
- [ ] Unsupported/missing spectra render warning banners, not crashes.
- [ ] Graceful degradation if chemistry kernel surface is not integrated.

---

## [Batch 6] Renderer Diagnostics, Error States, and Smoke Readiness

### 目标
Provide frontend entry points for diagnostics export, rebuild controls, and systematic error state rendering. Ensure renderer is smoke-ready.

### 为什么现在做
This batch makes the renderer self-supporting: users can diagnose issues and trigger rebuilds without developer tools. It also validates that all error paths are reachable and presentable.

### Renderer / UI 要补什么

1. **`diagnostics-page.js`** — Diagnostics and rebuild control center:
   - **Diagnostics section**:
     - "Export Support Bundle" button.
     - Output path input (plain text; host gap for save-dialog exists).
     - Export progress / result display.
     - Last export path and timestamp.
   - **Rebuild section**:
     - Current rebuild status from `rebuild.getStatus`.
     - "Start Rebuild" button (disabled if rebuild already in-flight).
     - "Wait for Rebuild" button (for manual wait trigger).
     - Rebuild log / result display.
   - **Runtime section**:
     - Formatted `runtime.getSummary()` dump for support.

2. **`diagnostics-export-card.js`** — Export workflow:
   - Input for `outputPath`.
   - **Temporary input strategy (Baseline v1):** Plain text input for `outputPath`. This is accepted as a temporary measure while `EXPLICIT-HOST-GAP-010` (save dialog) remains unfilled. Batch 6 must not be blocked waiting for the host dialog API.
   - Submit calls `diagnostics.exportSupportBundle({ outputPath })`.
   - Success: show exported path.
   - Failure: show `host-error-card` with retry.

3. **`rebuild-control-card.js`** — Rebuild workflow:
   - Status indicator from `rebuild.getStatus`.
   - Start button → `rebuild.start()`.
   - Wait button → `rebuild.wait({ timeoutMs })` with timeout input.
   - Result display.

4. **`error-state-catalog.js`** — Systematic error surface testing:
   - A debug/test-only page (or hidden section) that forces each error state:
     - Host unavailable (disconnect simulation).
     - Session not open (close vault).
     - Degraded runtime (mock or wait for catching_up).
     - Rebuild in-flight (trigger rebuild).
     - Diagnostics export failed (invalid output path).
   - This ensures all `state-surface` variants are visually verifiable.

5. **`smoke-readiness.js`** — Renderer smoke harness:
   - Exposes `window.__rendererSmoke` object with functions:
     - `getPageName()` — returns current page identifier.
     - `navigateTo(pageName)` — programmatic nav for smoke tests.
     - `getHostStoreSnapshot()` — returns current store state.
   - Smoke tests use these instead of raw `executeJavaScript` DOM queries where possible.

### 明确不做什么
- No analytics or telemetry collection.
- No complex error reporting platform (Sentry, etc.).
- No product-level support chat/ticket system.
- No auto-upload of support bundles.

### 主要消费哪些现有 `window.hostShell.*` API
- `diagnostics.exportSupportBundle({ outputPath })`
- `rebuild.getStatus()`
- `rebuild.start()`
- `rebuild.wait({ timeoutMs })`
- `runtime.getSummary()`
- `session.getStatus()`

### 是否存在 host gap
- **Host Gap R-6A:** No save-dialog API in preload. Renderer cannot ask user where to save support bundle without plain text input or new host API.
- **Host Gap R-6B:** `diagnostics.exportSupportBundle` result shape is not documented. Renderer needs `outputPath`, `sizeBytes`, `exportedAt` to confirm success.

### 若有 host gap，必须单列为“显式依赖项”
- `EXPLICIT-HOST-GAP-010`: `host.dialog.showSaveDialog()` or equivalent exposed through preload for support bundle save location selection.
- `EXPLICIT-HOST-GAP-011`: Stable `diagnostics.exportSupportBundle` response schema with `outputPath`, `sizeBytes`, `exportedAt`.

### 需要补哪些 renderer-side tests
- `diagnostics-export-card.test.js`: calls `diagnostics.exportSupportBundle` with input path; renders success/failure correctly.
- `rebuild-control-card.test.js`: disables start button when status shows in-flight.
- `error-state-catalog.test.js`: each forced error state renders correct surface variant.

### 需要补哪些 integration / smoke checks
- Smoke navigates to Diagnostics, clicks Export Support Bundle, asserts success path renders exported path.
- Smoke triggers rebuild start, asserts status updates to in-flight.
- Smoke triggers rebuild wait, asserts result renders after completion.
- Smoke verifies all four `state-surface` variants are reachable:
  - Loading: search in progress.
  - Empty: empty search query.
  - Unavailable: host unavailable simulation.
  - Error: invalid search argument.

### 需要补哪些前端状态与错误处理规则
- If `diagnostics.exportSupportBundle` fails with `HOST_INVALID_ARGUMENT` → highlight outputPath input, show inline error.
- If `diagnostics.exportSupportBundle` fails with `HOST_KERNEL_IO_ERROR` → show `host-error-card`: "Could not write support bundle. Check disk space and permissions."
- If `rebuild.start` fails with `HOST_REBUILD_ALREADY_RUNNING` → show info banner: "Rebuild is already in progress."
- If `rebuild.wait` times out → show result: "Rebuild did not complete within timeout. Check status."
- All error banners auto-dismiss after 10s unless they require user action.

### 验收标准
- [ ] Diagnostics page is reachable from vault page nav.
- [ ] Export support bundle workflow completes end-to-end.
- [ ] Rebuild start/wait/status workflow completes end-to-end.
- [ ] All four base state surfaces (loading, empty, unavailable, error) are visually verified in smoke.
- [ ] `window.__rendererSmoke` exposes navigation and snapshot helpers.
- [ ] Renderer smoke can validate all pages without `executeJavaScript` DOM scraping.

---

## 建议执行顺序（按周 / 批次）

| 周 | 批次 | 重点 | 前提 |
|---|---|---|---|
| **Week 1** | Batch 1 | App shell, routing, welcome page, vault open/close | None; purely renderer-side. Host APIs already sealed. |
| **Week 1** | Batch 2 (partial) | Runtime badge + session status card + polling | Batch 1 complete (needs vault page to mount badge). |
| **Week 2** | Batch 2 (completion) | Runtime page, degraded banners, error thresholds | Batch 1 stable. |
| **Week 2** | Batch 3 | Search page, results, pagination, filters | Batch 1 + Batch 2 (needs session open + runtime ready). |
| **Week 3** | Batch 4 | Attachment list, detail, PDF metadata, refs | Batch 1 + Batch 3 (search validates read path first). |
| **Week 3** | Batch 5 | Chemistry spectra list, detail, note refs | Batch 4 complete (attachment refs pattern is reusable). |
| **Week 4** | Batch 6 | Diagnostics export, rebuild controls, smoke readiness | All prior batches complete. This is integration + validation. |

### 页面/状态优先级

1. **先做的页面：** Welcome → Vault (shell) → Search. These give users immediate functional value.
2. **次做的页面:** Attachments → Chemistry. These are domain-specific and reuse patterns from Search.
3. **最后做的页面:** Diagnostics. This is operational/support surface, not core UX.
4. **先做状态:** Unavailable → Loading → Empty → Error. Unavailable is the foundation; every other feature builds on top of it.

### 并行工作streams
- **Stream A (Shell + State):** Batch 1 + Batch 2. One owner.
- **Stream B (Content Pages):** Batch 3 + Batch 4 + Batch 5. One owner, sequential because patterns transfer.
- **Stream C (Smoke + Diagnostics):** Batch 6. One owner, starts after Week 2.

### 阻塞关系
- Batch 3 is blocked until Batch 1 `vault-page` exists (search needs a shell to render in).
- Batch 4 is blocked until Batch 3 search `view-model` pattern is established.
- Batch 5 is blocked until Batch 4 attachment refs pattern is established.
- Batch 6 is blocked until all pages exist so smoke can navigate end-to-end.
- All batches are blocked on **Explicit Host Gaps** if those gaps are filed; renderer must not proceed past placeholder UI for gap-dependent features.
