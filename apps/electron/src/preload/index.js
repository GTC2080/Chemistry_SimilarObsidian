const { contextBridge, ipcRenderer } = require("electron");

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

const HOST_ERROR_CODES = Object.freeze({
  bridgeProtocolError: "HOST_BRIDGE_PROTOCOL_ERROR",
  ipcInvokeFailed: "HOST_IPC_INVOKE_FAILED"
});

// Keep the preload self-contained so it stays compatible with sandbox=true.
const DESKTOP_IPC_CHANNELS = Object.freeze({
  windowMinimize: "desktop/window/minimize",
  windowToggleMaximize: "desktop/window/toggle-maximize",
  windowClose: "desktop/window/close",
  dialogPickDirectory: "desktop/dialog/pick-directory",
  appGetVersion: "desktop/app/get-version"
});

function sanitizeEnvelope(envelope) {
  if (!envelope || typeof envelope.ok !== "boolean" || !("data" in envelope) || !("error" in envelope)) {
    return {
      ok: false,
      data: null,
      error: {
        code: HOST_ERROR_CODES.bridgeProtocolError,
        message: "Host returned an invalid IPC envelope.",
        details: null
      }
    };
  }

  return {
    ok: envelope.ok,
    data: envelope.data ?? null,
    error: envelope.error ?? null,
    ...(typeof envelope.request_id === "string" ? { request_id: envelope.request_id } : {})
  };
}

async function invokeHost(channel, payload = {}) {
  try {
    const response = await ipcRenderer.invoke(channel, payload);
    return sanitizeEnvelope(response);
  } catch {
    return {
      ok: false,
      data: null,
      error: {
        code: HOST_ERROR_CODES.ipcInvokeFailed,
        message: "IPC invocation failed.",
        details: null
      }
    };
  }
}

async function invokeDesktop(channel, payload = {}) {
  return ipcRenderer.invoke(channel, payload);
}

const hostShell = Object.freeze({
  bootstrap: Object.freeze({
    async getInfo(requestId) {
      return invokeHost(HOST_IPC_CHANNELS.bootstrapGetInfo, {
        ...(requestId ? { request_id: requestId } : {})
      });
    }
  }),
  runtime: Object.freeze({
    async getSummary(requestId) {
      return invokeHost(HOST_IPC_CHANNELS.runtimeGetSummary, {
        ...(requestId ? { request_id: requestId } : {})
      });
    }
  }),
  session: Object.freeze({
    async getStatus(requestId) {
      return invokeHost(HOST_IPC_CHANNELS.sessionGetStatus, {
        ...(requestId ? { request_id: requestId } : {})
      });
    },

    async openVault(vaultPath, requestId) {
      return invokeHost(HOST_IPC_CHANNELS.sessionOpenVault, {
        vaultPath,
        ...(requestId ? { request_id: requestId } : {})
      });
    },

    async closeVault(requestId) {
      return invokeHost(HOST_IPC_CHANNELS.sessionCloseVault, {
        ...(requestId ? { request_id: requestId } : {})
      });
    }
  }),
  files: Object.freeze({
    async listEntries(request = {}, requestId) {
      return invokeHost(HOST_IPC_CHANNELS.filesListEntries, {
        ...request,
        ...(requestId ? { request_id: requestId } : {})
      });
    },

    async readNote(request = {}, requestId) {
      return invokeHost(HOST_IPC_CHANNELS.filesReadNote, {
        ...request,
        ...(requestId ? { request_id: requestId } : {})
      });
    },

    async writeNote(request = {}, requestId) {
      return invokeHost(HOST_IPC_CHANNELS.filesWriteNote, {
        ...request,
        ...(requestId ? { request_id: requestId } : {})
      });
    },

    async listRecent(request = {}, requestId) {
      return invokeHost(HOST_IPC_CHANNELS.filesListRecent, {
        ...request,
        ...(requestId ? { request_id: requestId } : {})
      });
    }
  }),
  search: Object.freeze({
    async query(request = {}, requestId) {
      return invokeHost(HOST_IPC_CHANNELS.searchQuery, {
        ...request,
        ...(requestId ? { request_id: requestId } : {})
      });
    }
  }),
  attachments: Object.freeze({
    async list(request = {}, requestId) {
      return invokeHost(HOST_IPC_CHANNELS.attachmentsList, {
        ...request,
        ...(requestId ? { request_id: requestId } : {})
      });
    },

    async get(request = {}, requestId) {
      return invokeHost(HOST_IPC_CHANNELS.attachmentsGet, {
        ...request,
        ...(requestId ? { request_id: requestId } : {})
      });
    },

    async queryNoteRefs(request = {}, requestId) {
      return invokeHost(HOST_IPC_CHANNELS.attachmentsQueryNoteRefs, {
        ...request,
        ...(requestId ? { request_id: requestId } : {})
      });
    },

    async queryReferrers(request = {}, requestId) {
      return invokeHost(HOST_IPC_CHANNELS.attachmentsQueryReferrers, {
        ...request,
        ...(requestId ? { request_id: requestId } : {})
      });
    }
  }),
  pdf: Object.freeze({
    async getMetadata(request = {}, requestId) {
      return invokeHost(HOST_IPC_CHANNELS.pdfGetMetadata, {
        ...request,
        ...(requestId ? { request_id: requestId } : {})
      });
    },

    async queryNoteSourceRefs(request = {}, requestId) {
      return invokeHost(HOST_IPC_CHANNELS.pdfQueryNoteSourceRefs, {
        ...request,
        ...(requestId ? { request_id: requestId } : {})
      });
    },

    async queryReferrers(request = {}, requestId) {
      return invokeHost(HOST_IPC_CHANNELS.pdfQueryReferrers, {
        ...request,
        ...(requestId ? { request_id: requestId } : {})
      });
    }
  }),
  chemistry: Object.freeze({
    async queryMetadata(request = {}, requestId) {
      return invokeHost(HOST_IPC_CHANNELS.chemistryQueryMetadata, {
        ...request,
        ...(requestId ? { request_id: requestId } : {})
      });
    },

    async listSpectra(request = {}, requestId) {
      return invokeHost(HOST_IPC_CHANNELS.chemistryListSpectra, {
        ...request,
        ...(requestId ? { request_id: requestId } : {})
      });
    },

    async getSpectrum(request = {}, requestId) {
      return invokeHost(HOST_IPC_CHANNELS.chemistryGetSpectrum, {
        ...request,
        ...(requestId ? { request_id: requestId } : {})
      });
    },

    async queryNoteRefs(request = {}, requestId) {
      return invokeHost(HOST_IPC_CHANNELS.chemistryQueryNoteRefs, {
        ...request,
        ...(requestId ? { request_id: requestId } : {})
      });
    },

    async queryReferrers(request = {}, requestId) {
      return invokeHost(HOST_IPC_CHANNELS.chemistryQueryReferrers, {
        ...request,
        ...(requestId ? { request_id: requestId } : {})
      });
    }
  }),
  diagnostics: Object.freeze({
    async exportSupportBundle(request = {}, requestId) {
      return invokeHost(HOST_IPC_CHANNELS.diagnosticsExportSupportBundle, {
        ...request,
        ...(requestId ? { request_id: requestId } : {})
      });
    }
  }),
  rebuild: Object.freeze({
    async getStatus(requestId) {
      return invokeHost(HOST_IPC_CHANNELS.rebuildGetStatus, {
        ...(requestId ? { request_id: requestId } : {})
      });
    },

    async start(requestId) {
      return invokeHost(HOST_IPC_CHANNELS.rebuildStart, {
        ...(requestId ? { request_id: requestId } : {})
      });
    },

    async wait(request = {}, requestId) {
      return invokeHost(HOST_IPC_CHANNELS.rebuildWait, {
        ...request,
        ...(requestId ? { request_id: requestId } : {})
      });
    }
  })
});

contextBridge.exposeInMainWorld("hostShell", hostShell);

contextBridge.exposeInMainWorld("desktopShell", Object.freeze({
  window: Object.freeze({
    minimize() {
      return invokeDesktop(DESKTOP_IPC_CHANNELS.windowMinimize);
    },
    toggleMaximize() {
      return invokeDesktop(DESKTOP_IPC_CHANNELS.windowToggleMaximize);
    },
    close() {
      return invokeDesktop(DESKTOP_IPC_CHANNELS.windowClose);
    }
  }),
  dialog: Object.freeze({
    async pickDirectory() {
      const result = await invokeDesktop(DESKTOP_IPC_CHANNELS.dialogPickDirectory);
      return result?.ok ? (result.data ?? null) : null;
    }
  }),
  app: Object.freeze({
    async getVersion() {
      const result = await invokeDesktop(DESKTOP_IPC_CHANNELS.appGetVersion);
      return result?.ok ? (result.data ?? null) : null;
    }
  })
}));
