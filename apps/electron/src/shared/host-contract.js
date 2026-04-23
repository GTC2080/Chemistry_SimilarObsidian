const HOST_IPC_CHANNELS = Object.freeze({
  bootstrapGetInfo: "host/bootstrap/get-info",
  runtimeGetSummary: "host/runtime/get-summary",
  sessionGetStatus: "host/session/get-status",
  sessionOpenVault: "host/session/open-vault",
  sessionCloseVault: "host/session/close-vault",
  filesListEntries: "host/files/list-entries",
  filesReadNote: "host/files/read-note",
  filesWriteNote: "host/files/write-note",
  filesListRecent: "host/files/list-recent",
  searchQuery: "host/search/query",
  attachmentsList: "host/attachments/list",
  attachmentsGet: "host/attachments/get",
  attachmentsQueryNoteRefs: "host/attachments/query-note-refs",
  attachmentsQueryReferrers: "host/attachments/query-referrers",
  pdfGetMetadata: "host/pdf/get-metadata",
  pdfQueryNoteSourceRefs: "host/pdf/query-note-source-refs",
  pdfQueryReferrers: "host/pdf/query-referrers",
  chemistryQueryMetadata: "host/chemistry/query-metadata",
  chemistryListSpectra: "host/chemistry/list-spectra",
  chemistryGetSpectrum: "host/chemistry/get-spectrum",
  chemistryQueryNoteRefs: "host/chemistry/query-note-refs",
  chemistryQueryReferrers: "host/chemistry/query-referrers",
  diagnosticsExportSupportBundle: "host/diagnostics/export-support-bundle",
  rebuildGetStatus: "host/rebuild/get-status",
  rebuildStart: "host/rebuild/start",
  rebuildWait: "host/rebuild/wait"
});

const HOST_API_GROUPS = Object.freeze([
  "bootstrap",
  "runtime",
  "session",
  "files",
  "search",
  "attachments",
  "pdf",
  "chemistry",
  "diagnostics",
  "rebuild"
]);

const SECURITY_BASELINE = Object.freeze({
  contextIsolation: true,
  nodeIntegration: false,
  enableRemoteModule: false,
  sandbox: true,
  rendererDirectNodeAccess: false,
  preloadIsOnlyBridge: true
});

const HOST_LIFECYCLE_STATES = Object.freeze({
  booting: "booting",
  ready: "ready",
  shuttingDown: "shutting_down",
  closed: "closed"
});

const HOST_SESSION_STATES = Object.freeze({
  none: "none",
  opening: "opening",
  open: "open",
  closing: "closing"
});

const HOST_INDEX_STATES = Object.freeze({
  unavailable: "unavailable",
  catchingUp: "catching_up",
  ready: "ready",
  rebuilding: "rebuilding"
});

const HOST_ERROR_CODES = Object.freeze({
  invalidArgument: "HOST_INVALID_ARGUMENT",
  busy: "HOST_SESSION_BUSY",
  sessionNotOpen: "HOST_SESSION_NOT_OPEN",
  kernelAdapterUnavailable: "HOST_KERNEL_ADAPTER_UNAVAILABLE",
  kernelBindingNotFound: "HOST_KERNEL_BINDING_NOT_FOUND",
  kernelBindingLoadFailed: "HOST_KERNEL_BINDING_LOAD_FAILED",
  kernelSurfaceNotIntegrated: "HOST_KERNEL_SURFACE_NOT_INTEGRATED",
  kernelNotFound: "HOST_KERNEL_NOT_FOUND",
  kernelConflict: "HOST_KERNEL_CONFLICT",
  kernelIoError: "HOST_KERNEL_IO_ERROR",
  kernelTimeout: "HOST_KERNEL_TIMEOUT",
  rebuildAlreadyRunning: "HOST_REBUILD_ALREADY_RUNNING",
  rebuildNotInFlight: "HOST_REBUILD_NOT_IN_FLIGHT",
  bridgeProtocolError: "HOST_BRIDGE_PROTOCOL_ERROR",
  ipcInvokeFailed: "HOST_IPC_INVOKE_FAILED",
  internalError: "HOST_INTERNAL_ERROR"
});

module.exports = {
  HOST_API_GROUPS,
  HOST_ERROR_CODES,
  HOST_INDEX_STATES,
  HOST_IPC_CHANNELS,
  HOST_LIFECYCLE_STATES,
  HOST_SESSION_STATES,
  SECURITY_BASELINE
};
