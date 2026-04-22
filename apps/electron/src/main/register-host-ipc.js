const { ipcMain } = require("electron");
const { HOST_IPC_CHANNELS } = require("../shared/host-contract");
const { fail, ok } = require("./host-runtime");

function registerHostIpc(hostRuntime, hostApi) {
  ipcMain.handle(HOST_IPC_CHANNELS.bootstrapGetInfo, async (_event, payload = {}) => {
    try {
      return ok(hostRuntime.getBootstrapInfo(), payload.request_id);
    } catch {
      return fail("HOST_INTERNAL_ERROR", "Failed to read bootstrap info.", null, payload.request_id);
    }
  });

  ipcMain.handle(HOST_IPC_CHANNELS.runtimeGetSummary, async (_event, payload = {}) => {
    try {
      return ok(hostRuntime.getRuntimeSummary(), payload.request_id);
    } catch {
      return fail("HOST_INTERNAL_ERROR", "Failed to read runtime summary.", null, payload.request_id);
    }
  });

  ipcMain.handle(HOST_IPC_CHANNELS.sessionGetStatus, async (_event, payload = {}) => {
    return hostRuntime.getSessionStatus(payload);
  });

  ipcMain.handle(HOST_IPC_CHANNELS.sessionOpenVault, async (_event, payload = {}) => {
    return hostRuntime.openVault(payload);
  });

  ipcMain.handle(HOST_IPC_CHANNELS.sessionCloseVault, async (_event, payload = {}) => {
    return hostRuntime.closeVault(payload);
  });

  ipcMain.handle(HOST_IPC_CHANNELS.searchQuery, async (_event, payload = {}) => {
    return hostApi.querySearch(payload);
  });

  ipcMain.handle(HOST_IPC_CHANNELS.attachmentsList, async (_event, payload = {}) => {
    return hostApi.listAttachments(payload);
  });

  ipcMain.handle(HOST_IPC_CHANNELS.attachmentsGet, async (_event, payload = {}) => {
    return hostApi.getAttachment(payload);
  });

  ipcMain.handle(HOST_IPC_CHANNELS.attachmentsQueryNoteRefs, async (_event, payload = {}) => {
    return hostApi.queryNoteAttachmentRefs(payload);
  });

  ipcMain.handle(HOST_IPC_CHANNELS.attachmentsQueryReferrers, async (_event, payload = {}) => {
    return hostApi.queryAttachmentReferrers(payload);
  });

  ipcMain.handle(HOST_IPC_CHANNELS.pdfGetMetadata, async (_event, payload = {}) => {
    return hostApi.getPdfMetadata(payload);
  });

  ipcMain.handle(HOST_IPC_CHANNELS.pdfQueryNoteSourceRefs, async (_event, payload = {}) => {
    return hostApi.queryNotePdfSourceRefs(payload);
  });

  ipcMain.handle(HOST_IPC_CHANNELS.pdfQueryReferrers, async (_event, payload = {}) => {
    return hostApi.queryPdfReferrers(payload);
  });

  ipcMain.handle(HOST_IPC_CHANNELS.chemistryQueryMetadata, async (_event, payload = {}) => {
    return hostApi.queryChemSpectrumMetadata(payload);
  });

  ipcMain.handle(HOST_IPC_CHANNELS.chemistryListSpectra, async (_event, payload = {}) => {
    return hostApi.listChemSpectra(payload);
  });

  ipcMain.handle(HOST_IPC_CHANNELS.chemistryGetSpectrum, async (_event, payload = {}) => {
    return hostApi.getChemSpectrum(payload);
  });

  ipcMain.handle(HOST_IPC_CHANNELS.chemistryQueryNoteRefs, async (_event, payload = {}) => {
    return hostApi.queryNoteChemSpectrumRefs(payload);
  });

  ipcMain.handle(HOST_IPC_CHANNELS.chemistryQueryReferrers, async (_event, payload = {}) => {
    return hostApi.queryChemSpectrumReferrers(payload);
  });

  ipcMain.handle(HOST_IPC_CHANNELS.diagnosticsExportSupportBundle, async (_event, payload = {}) => {
    return hostApi.exportSupportBundle(payload);
  });

  ipcMain.handle(HOST_IPC_CHANNELS.rebuildGetStatus, async (_event, payload = {}) => {
    return hostApi.getRebuildStatus(payload);
  });

  ipcMain.handle(HOST_IPC_CHANNELS.rebuildStart, async (_event, payload = {}) => {
    return hostApi.startRebuild(payload);
  });

  ipcMain.handle(HOST_IPC_CHANNELS.rebuildWait, async (_event, payload = {}) => {
    return hostApi.waitForRebuild(payload);
  });
}

module.exports = {
  registerHostIpc
};
