# Integration Plan

## Current Split

- `kernel/`
  - sealed kernel platform
- `apps/electron/`
  - future Electron host entry

## Responsibility Split

- kernel work continues inside `kernel/`
- Electron host integration lands under `apps/electron/src/main/` and `apps/electron/src/preload/`
- current Nexus renderer work lands under `apps/electron/src/renderer-react/`
- legacy smoke-only renderer assets remain separate from the product renderer path

## Current Rule

This repository reorganization started with the Electron host shell baseline and now moves into real kernel adapter integration.

The sealed kernel platform remains unchanged. Electron host integration consumes the sealed C ABI through a host-only adapter boundary.

Current integration status:

- real native binding loading is in place
- runtime/session are wired to the sealed kernel
- read surfaces for search, attachments, PDF, and chemistry are wired through the host adapter
- diagnostics export and rebuild start/status/wait are wired through the host adapter
- host-side regression tests now cover adapter request shaping and native binding resolution
- packaged-mode native binding resolution now has a dedicated readiness check
- packaged-run smoke now validates a packaged Electron shell plus native adapter load path

## Current Renderer Handoff: Content Workspace Baseline

Renderer has now moved past the launcher/dashboard stage and into a content-centric workspace baseline.

Current renderer status:

- Vault Launcher is usable and semantically separated from workspace mode
- Workspace Shell is content-first rather than tool-first
- Files owns the default center stage after vault open
- Search / Attachments / Chemistry / Diagnostics remain reachable as tool surfaces
- current content selection inside Files is now backed by host-facing Files baseline reads
- Nexus-style renderer shell is active under `apps/electron/src/renderer-react/`
- Attachments / PDF / Chemistry consume real host surfaces and keep raw contract fields behind developer-detail sections
- Diagnostics / Rebuild are low-frequency support surfaces, not primary workspace content

Current Files baseline integration status:

- host now exposes:
  - `window.hostShell.files.listEntries(...)`
  - `window.hostShell.files.readNote(...)`
  - `window.hostShell.files.listRecent(...)`
- Files baseline stays host-truth-backed:
  - renderer does not read vault files directly
  - renderer does not infer note bodies from unrelated APIs
  - renderer does not reconstruct a file tree from search or attachment surfaces

This closes the three initial Files host gaps that blocked the content workspace baseline:

- `EXPLICIT-HOST-GAP-FILES-TREE-SURFACE`
- `EXPLICIT-HOST-GAP-CURRENT-FILE-READ-SURFACE`
- `EXPLICIT-HOST-GAP-RECENT-CONTENT-SURFACE`

## Current Renderer Boundary Rule

With the Files baseline now present:

- renderer owns layout, selection shell, and host-backed content presentation
- no renderer workaround may bypass `window.hostShell.*`
- no new Files semantics may be reverse-engineered from SQLite, disk layout, support bundle internals, or unrelated host APIs

## Canonical Build Rule

- all validation still runs from `kernel/`
- all sealed-node verification still uses:
  - `ctest`
  - benchmark gates
  - `kernel_phase_gate`

## Real Adapter Execution Constraints

- native binding mechanism is fixed to a Node-API addon
- native binding build path is fixed to CMake so host integration stays on the existing Windows/MSVC/CMake toolchain
- Windows native linking uses the Electron-version-matched `node.lib` from the official Electron headers distribution
- raw binding is main-process only and never crosses into preload or renderer
- main process holds a single adapter instance; renderer only consumes `window.hostShell.*`
- execution model is fixed as:
  - host runtime state and IPC stay in main process
  - native kernel calls are treated as adapter-owned work and must not leak blocking semantics to renderer contracts
  - adapter integration proceeds behind async host APIs; renderer never waits on raw C ABI calls directly
  - long-running paths such as diagnostics export and rebuild wait are adapter-managed operations, not renderer-managed flows
- C ABI memory ownership is fixed as:
  - raw kernel-owned structs and buffers are converted to host models inside the adapter
  - all `kernel_free_*` calls happen before the IPC envelope is returned
  - preload and renderer never observe C ABI ownership or free semantics
- `HOST_KERNEL_ADAPTER_UNAVAILABLE` is reserved for:
  - binding not found
  - binding load failure
  - adapter not yet established
- once the binding is loaded, unwired host surfaces must fail with more specific host-visible errors instead of reusing `HOST_KERNEL_ADAPTER_UNAVAILABLE`
- once a binding is loaded but no vault session is active, host read surfaces must fail with `HOST_SESSION_NOT_OPEN`
