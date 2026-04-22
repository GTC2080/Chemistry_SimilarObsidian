# Electron Host

This directory is the Electron host project for Chemistry_Obsidian.

Current state:

- Electron is installed as the only framework dependency
- `src/main/` owns the main-process baseline and host runtime state
- `src/preload/` exposes a narrow async host API through a single bridge
- `src/shared/` freezes the current host IPC contract and security baseline
- native binding integration is frozen to a Node-API addon loaded only inside the host-side adapter boundary
- the native binding is built through CMake to stay aligned with the sealed kernel's Windows/MSVC toolchain
- runtime/session now run through the real kernel adapter
- search, attachments, PDF, chemistry, diagnostics export, and rebuild all have real host-side wiring behind the preload bridge
- `renderer` only shows a plain placeholder page backed by that host bridge
- host-side regression checks now exist for adapter request shaping and native binding resolution
- packaged-mode native binding resolution now has a dedicated baseline check

What this shell is for:

- anchoring the Electron main process
- anchoring the preload bridge boundary
- freezing the current IPC contract entry
- freezing the current kernel adapter boundary

What is not done yet:

- no renderer feature UI
- no React/Vue/Svelte stack
- no real packaged app bundle yet
- no release polish

Current bridge groups:

- `hostShell.bootstrap`
- `hostShell.runtime`
- `hostShell.session`
- `hostShell.search`
- `hostShell.attachments`
- `hostShell.pdf`
- `hostShell.chemistry`
- `hostShell.diagnostics`
- `hostShell.rebuild`

Current envelope rules:

- every host call resolves to `{ ok, data, error, request_id? }`
- preload is the only renderer bridge
- renderer does not touch Node.js, Electron primitives, SQLite, state files, or native kernel details directly

Current native binding rules:

- native binding mechanism is fixed to a Node-API addon
- native binding build path is fixed to CMake, not `node-gyp`
- Windows linking uses the Electron-version-matched `node.lib` from the official Electron headers distribution
- raw binding only exists inside the main-process adapter boundary
- adapter owns C ABI result conversion and C ABI free calls before any host envelope is returned
- `HOST_KERNEL_ADAPTER_UNAVAILABLE` is reserved for initialization-level adapter absence only
- once the binding is loaded, not-yet-wired surfaces fail with `HOST_KERNEL_SURFACE_NOT_INTEGRATED`

Current session behavior:

- `hostShell.session.getStatus()` is available
- `hostShell.session.openVault()` now opens a real sealed-kernel vault session
- `hostShell.session.closeVault()` closes the real session and resolves to `already_closed` when no session is active
- host read surfaces fail with `HOST_SESSION_NOT_OPEN` until a vault session is active

Current adapter behavior:

- search, attachments, PDF, chemistry, diagnostics export, and rebuild control now consume the sealed kernel through the real adapter boundary
- initialization-level binding problems still fail with binding-load level host errors
- `HOST_KERNEL_SURFACE_NOT_INTEGRATED` is now reserved for host surfaces that are genuinely not wired yet
- `npm run smoke` now checks pre-open failures, real vault open/close, real read surfaces, diagnostics export, and rebuild completion on a seeded smoke vault

Current validation entry:

- `npm run smoke`
- `npm run build:native`
- `npm test`
- `npm run check:packaged-resolution`

Renderer and UI work will be added later by Kimi.
