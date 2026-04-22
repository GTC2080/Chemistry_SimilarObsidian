# Electron App Placeholder

This directory is a placeholder for future Electron host integration.

Current status:

- not a running Electron application
- no preload implementation
- no IPC implementation
- no renderer implementation

Directory intent:

- `src/main/`
  - future Electron main-process host integration
- `src/preload/`
  - future preload bridge
- `src/renderer/`
  - future renderer entrypoint

Current boundary:

- renderer implementation is deferred
- kernel contracts remain owned by `kernel/`
- this placeholder does not change the sealed kernel platform
