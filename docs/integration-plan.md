# Integration Plan

## Current Split

- `kernel/`
  - sealed kernel platform
- `apps/electron/`
  - future Electron host entry

## Responsibility Split

- kernel work continues inside `kernel/`
- Electron host integration lands under `apps/electron/src/main/` and `apps/electron/src/preload/`
- renderer work lands under `apps/electron/src/renderer/`

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
