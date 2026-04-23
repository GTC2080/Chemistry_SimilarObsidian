const { app } = require("electron");
const {
  FilesSurfaceError,
  FILES_LIST_LIMIT_MAX,
  createFolder,
  deleteEntry,
  listEntries,
  listRecentNotes,
  readNote,
  renameEntry
} = require("./files-host-surface");
const { createKernelAdapterBoundary } = require("./kernel-adapter/kernel-adapter-boundary");
const {
  mapAttachmentList,
  mapAttachmentRecord,
  mapAttachmentReferrers,
  mapChemSpectrumList,
  mapChemSpectrumRecord,
  mapChemSpectrumReferrers,
  mapChemSpectrumSourceRefs,
  mapDiagnosticsExport,
  mapDomainMetadataList,
  mapFileNoteRecord,
  mapFileOperationResult,
  mapFilesList,
  mapKernelRebuildStatus,
  mapNoteWriteResult,
  mapPdfMetadata,
  mapPdfReferrers,
  mapPdfSourceRefs,
  mapSearchPage
} = require("./host-model-mappers");
const { fail, ok } = require("./host-runtime");
const { HOST_ERROR_CODES } = require("../shared/host-contract");

const SEARCH_LIMIT_MAX = 128;
const LIST_LIMIT_DEFAULT = 64;
const REBUILD_WAIT_DEFAULT_MS = 30000;

function getRunMode() {
  return app && app.isPackaged ? "packaged" : "dev";
}

function sanitizeRequestId(payload) {
  return payload && typeof payload.request_id === "string" && payload.request_id.trim()
    ? payload.request_id.trim()
    : null;
}

function ensureNonEmptyString(value, fieldName, requestId) {
  if (typeof value !== "string" || !value.trim()) {
    return fail(
      HOST_ERROR_CODES.invalidArgument,
      `${fieldName} must be a non-empty string.`,
      {
        field: fieldName
      },
      requestId
    );
  }

  return value.trim();
}

function ensureTextString(value, fieldName, requestId) {
  if (typeof value !== "string") {
    return fail(
      HOST_ERROR_CODES.invalidArgument,
      `${fieldName} must be a string.`,
      {
        field: fieldName
      },
      requestId
    );
  }

  return value;
}

function ensureLimit(value, requestId, fieldName = "limit", defaultValue = LIST_LIMIT_DEFAULT, maxValue = SEARCH_LIMIT_MAX) {
  if (value == null) {
    return defaultValue;
  }

  if (!Number.isInteger(value) || value <= 0 || value > maxValue) {
    return fail(
      HOST_ERROR_CODES.invalidArgument,
      `${fieldName} must be an integer between 1 and ${maxValue}.`,
      {
        field: fieldName,
        maxValue
      },
      requestId
    );
  }

  return value;
}

function ensureOffset(value, requestId) {
  if (value == null) {
    return 0;
  }

  if (!Number.isInteger(value) || value < 0) {
    return fail(
      HOST_ERROR_CODES.invalidArgument,
      "offset must be a non-negative integer.",
      {
        field: "offset"
      },
      requestId
    );
  }

  return value;
}

function finalizeAdapterResult(adapterResult, mapper, requestId, requestShape = null) {
  if (!adapterResult.ok) {
    return fail(
      adapterResult.error.code,
      adapterResult.error.message,
      adapterResult.error.details,
      requestId
    );
  }

  return ok(mapper(adapterResult.value, requestShape ?? {}), requestId);
}

class HostApi {
  constructor(options = {}) {
    this.kernelAdapter = options.kernelAdapter ?? createKernelAdapterBoundary();
    this.getActiveVaultPath = options.getActiveVaultPath ?? (() => null);
    this.filesSurface = options.filesSurface ?? {
      createFolder,
      deleteEntry,
      listEntries,
      readNote,
      listRecentNotes,
      renameEntry
    };
  }

  getKernelAdapter() {
    return this.kernelAdapter;
  }

  getBindingInfo() {
    return this.kernelAdapter.getBindingInfo();
  }

  getKernelRuntimeSummary() {
    return this.kernelAdapter.getKernelRuntimeSummary();
  }

  getRebuildSummary() {
    return this.kernelAdapter.getRebuildStatusSummary();
  }

  async querySearch(payload = {}) {
    const requestId = sanitizeRequestId(payload);
    const query = ensureNonEmptyString(payload.query, "query", requestId);
    if (typeof query !== "string") {
      return query;
    }

    const limit = ensureLimit(payload.limit, requestId, "limit", 25, SEARCH_LIMIT_MAX);
    if (typeof limit !== "number") {
      return limit;
    }

    const offset = ensureOffset(payload.offset, requestId);
    if (typeof offset !== "number") {
      return offset;
    }

    const requestShape = {
      query,
      limit,
      offset,
      kind: payload.kind ?? "all",
      tagFilter: typeof payload.tagFilter === "string" ? payload.tagFilter : null,
      pathPrefix: typeof payload.pathPrefix === "string" ? payload.pathPrefix : null,
      includeDeleted: Boolean(payload.includeDeleted),
      sortMode: payload.sortMode ?? "rel_path_asc"
    };

    const adapterResult = await this.kernelAdapter.querySearch(requestShape);
    return finalizeAdapterResult(adapterResult, mapSearchPage, requestId, requestShape);
  }

  async listFilesEntries(payload = {}) {
    const requestId = sanitizeRequestId(payload);
    const limit = ensureLimit(payload.limit, requestId, "limit", 64, FILES_LIST_LIMIT_MAX);
    if (typeof limit !== "number") {
      return limit;
    }

    const parentRelPath = payload.parentRelPath == null
      ? ""
      : ensureOptionalString(payload.parentRelPath, "parentRelPath", requestId);
    if (typeof parentRelPath !== "string") {
      return parentRelPath;
    }

    const vaultPath = this.getActiveVaultPath();
    if (!vaultPath) {
      return fail(
        HOST_ERROR_CODES.sessionNotOpen,
        "Open a vault session before using this host surface.",
        {
          operation: "files.list_entries",
          run_mode: getRunMode()
        },
        requestId
      );
    }

    try {
      const requestShape = { parentRelPath, limit };
      const result = await this.filesSurface.listEntries({
        vaultPath,
        parentRelPath,
        limit
      });
      return ok(mapFilesList(result, requestShape), requestId);
    } catch (error) {
      return mapFilesSurfaceFailure(error, requestId);
    }
  }

  async readNoteFile(payload = {}) {
    const requestId = sanitizeRequestId(payload);
    const relPath = ensureNonEmptyString(payload.relPath, "relPath", requestId);
    if (typeof relPath !== "string") {
      return relPath;
    }

    const adapterResult = await this.kernelAdapter.readNote({ relPath });
    return finalizeAdapterResult(adapterResult, mapFileNoteRecord, requestId);
  }

  async writeNoteFile(payload = {}) {
    const requestId = sanitizeRequestId(payload);
    const relPath = ensureNonEmptyString(payload.relPath, "relPath", requestId);
    if (typeof relPath !== "string") {
      return relPath;
    }

    const bodyText = ensureTextString(payload.bodyText, "bodyText", requestId);
    if (typeof bodyText !== "string") {
      return bodyText;
    }

    const expectedRevision = payload.expectedRevision == null || payload.expectedRevision === ""
      ? null
      : ensureOptionalString(payload.expectedRevision, "expectedRevision", requestId);
    if (expectedRevision !== null && typeof expectedRevision !== "string") {
      return expectedRevision;
    }

    const adapterResult = await this.kernelAdapter.writeNote({
      relPath,
      bodyText,
      expectedRevision
    });
    return finalizeAdapterResult(adapterResult, mapNoteWriteResult, requestId);
  }

  async listRecentFiles(payload = {}) {
    const requestId = sanitizeRequestId(payload);
    const limit = ensureLimit(payload.limit, requestId, "limit", 16, FILES_LIST_LIMIT_MAX);
    if (typeof limit !== "number") {
      return limit;
    }

    const vaultPath = this.getActiveVaultPath();
    if (!vaultPath) {
      return fail(
        HOST_ERROR_CODES.sessionNotOpen,
        "Open a vault session before using this host surface.",
        {
          operation: "files.list_recent",
          run_mode: getRunMode()
        },
        requestId
      );
    }

    try {
      const requestShape = { limit };
      const result = await this.filesSurface.listRecentNotes({
        vaultPath,
        limit
      });
      return ok(mapFilesList(result, requestShape), requestId);
    } catch (error) {
      return mapFilesSurfaceFailure(error, requestId);
    }
  }

  async createFolderEntry(payload = {}) {
    const requestId = sanitizeRequestId(payload);
    const parentRelPath = payload.parentRelPath == null
      ? ""
      : ensureOptionalString(payload.parentRelPath, "parentRelPath", requestId);
    if (typeof parentRelPath !== "string") {
      return parentRelPath;
    }

    const folderName = ensureNonEmptyString(payload.folderName, "folderName", requestId);
    if (typeof folderName !== "string") {
      return folderName;
    }

    const vaultPath = this.getActiveVaultPath();
    if (!vaultPath) {
      return fail(
        HOST_ERROR_CODES.sessionNotOpen,
        "Open a vault session before using this host surface.",
        {
          operation: "files.create_folder",
          run_mode: getRunMode()
        },
        requestId
      );
    }

    try {
      const result = await this.filesSurface.createFolder({
        vaultPath,
        parentRelPath,
        folderName
      });
      return ok(mapFileOperationResult(result), requestId);
    } catch (error) {
      return mapFilesSurfaceFailure(error, requestId);
    }
  }

  async renameFileEntry(payload = {}) {
    const requestId = sanitizeRequestId(payload);
    const fromRelPath = ensureNonEmptyString(payload.fromRelPath, "fromRelPath", requestId);
    if (typeof fromRelPath !== "string") {
      return fromRelPath;
    }

    const toRelPath = ensureNonEmptyString(payload.toRelPath, "toRelPath", requestId);
    if (typeof toRelPath !== "string") {
      return toRelPath;
    }

    const vaultPath = this.getActiveVaultPath();
    if (!vaultPath) {
      return fail(
        HOST_ERROR_CODES.sessionNotOpen,
        "Open a vault session before using this host surface.",
        {
          operation: "files.rename_entry",
          run_mode: getRunMode()
        },
        requestId
      );
    }

    try {
      const result = await this.filesSurface.renameEntry({
        vaultPath,
        fromRelPath,
        toRelPath
      });
      const reconcileFailure = await this.reconcileKernelAfterFileMutation("files.rename_entry", requestId);
      if (reconcileFailure) {
        return reconcileFailure;
      }

      return ok(mapFileOperationResult(result), requestId);
    } catch (error) {
      return mapFilesSurfaceFailure(error, requestId);
    }
  }

  async deleteFileEntry(payload = {}) {
    const requestId = sanitizeRequestId(payload);
    const relPath = ensureNonEmptyString(payload.relPath, "relPath", requestId);
    if (typeof relPath !== "string") {
      return relPath;
    }

    const vaultPath = this.getActiveVaultPath();
    if (!vaultPath) {
      return fail(
        HOST_ERROR_CODES.sessionNotOpen,
        "Open a vault session before using this host surface.",
        {
          operation: "files.delete_entry",
          run_mode: getRunMode()
        },
        requestId
      );
    }

    try {
      const result = await this.filesSurface.deleteEntry({
        vaultPath,
        relPath
      });
      const reconcileFailure = await this.reconcileKernelAfterFileMutation("files.delete_entry", requestId);
      if (reconcileFailure) {
        return reconcileFailure;
      }

      return ok(mapFileOperationResult(result), requestId);
    } catch (error) {
      return mapFilesSurfaceFailure(error, requestId);
    }
  }

  async reconcileKernelAfterFileMutation(operation, requestId) {
    const startResult = await this.kernelAdapter.startRebuild();
    if (!startResult.ok && startResult.error.code !== HOST_ERROR_CODES.rebuildAlreadyRunning) {
      return fail(
        startResult.error.code,
        startResult.error.message,
        {
          ...(startResult.error.details ?? {}),
          operation
        },
        requestId
      );
    }

    const waitResult = await this.kernelAdapter.waitForRebuild({ timeoutMs: REBUILD_WAIT_DEFAULT_MS });
    if (!waitResult.ok && waitResult.error.code !== HOST_ERROR_CODES.rebuildNotInFlight) {
      return fail(
        waitResult.error.code,
        waitResult.error.message,
        {
          ...(waitResult.error.details ?? {}),
          operation
        },
        requestId
      );
    }

    return null;
  }

  async listAttachments(payload = {}) {
    const requestId = sanitizeRequestId(payload);
    const limit = ensureLimit(payload.limit, requestId);
    if (typeof limit !== "number") {
      return limit;
    }

    const adapterResult = await this.kernelAdapter.listAttachments({ limit });
    return finalizeAdapterResult(adapterResult, mapAttachmentList, requestId);
  }

  async getAttachment(payload = {}) {
    const requestId = sanitizeRequestId(payload);
    const attachmentRelPath = ensureNonEmptyString(payload.attachmentRelPath, "attachmentRelPath", requestId);
    if (typeof attachmentRelPath !== "string") {
      return attachmentRelPath;
    }

    const adapterResult = await this.kernelAdapter.getAttachment({ attachmentRelPath });
    return finalizeAdapterResult(adapterResult, mapAttachmentRecord, requestId);
  }

  async queryNoteAttachmentRefs(payload = {}) {
    const requestId = sanitizeRequestId(payload);
    const noteRelPath = ensureNonEmptyString(payload.noteRelPath, "noteRelPath", requestId);
    if (typeof noteRelPath !== "string") {
      return noteRelPath;
    }

    const limit = ensureLimit(payload.limit, requestId);
    if (typeof limit !== "number") {
      return limit;
    }

    const adapterResult = await this.kernelAdapter.queryNoteAttachmentRefs({
      noteRelPath,
      limit
    });
    return finalizeAdapterResult(adapterResult, mapAttachmentList, requestId);
  }

  async queryAttachmentReferrers(payload = {}) {
    const requestId = sanitizeRequestId(payload);
    const attachmentRelPath = ensureNonEmptyString(payload.attachmentRelPath, "attachmentRelPath", requestId);
    if (typeof attachmentRelPath !== "string") {
      return attachmentRelPath;
    }

    const limit = ensureLimit(payload.limit, requestId);
    if (typeof limit !== "number") {
      return limit;
    }

    const requestShape = { attachmentRelPath, limit };
    const adapterResult = await this.kernelAdapter.queryAttachmentReferrers(requestShape);
    return finalizeAdapterResult(adapterResult, mapAttachmentReferrers, requestId, requestShape);
  }

  async getPdfMetadata(payload = {}) {
    const requestId = sanitizeRequestId(payload);
    const attachmentRelPath = ensureNonEmptyString(payload.attachmentRelPath, "attachmentRelPath", requestId);
    if (typeof attachmentRelPath !== "string") {
      return attachmentRelPath;
    }

    const adapterResult = await this.kernelAdapter.getPdfMetadata({ attachmentRelPath });
    return finalizeAdapterResult(adapterResult, mapPdfMetadata, requestId);
  }

  async queryNotePdfSourceRefs(payload = {}) {
    const requestId = sanitizeRequestId(payload);
    const noteRelPath = ensureNonEmptyString(payload.noteRelPath, "noteRelPath", requestId);
    if (typeof noteRelPath !== "string") {
      return noteRelPath;
    }

    const limit = ensureLimit(payload.limit, requestId);
    if (typeof limit !== "number") {
      return limit;
    }

    const requestShape = { noteRelPath, limit };
    const adapterResult = await this.kernelAdapter.queryNotePdfSourceRefs(requestShape);
    return finalizeAdapterResult(adapterResult, mapPdfSourceRefs, requestId, requestShape);
  }

  async queryPdfReferrers(payload = {}) {
    const requestId = sanitizeRequestId(payload);
    const attachmentRelPath = ensureNonEmptyString(payload.attachmentRelPath, "attachmentRelPath", requestId);
    if (typeof attachmentRelPath !== "string") {
      return attachmentRelPath;
    }

    const limit = ensureLimit(payload.limit, requestId);
    if (typeof limit !== "number") {
      return limit;
    }

    const requestShape = { attachmentRelPath, limit };
    const adapterResult = await this.kernelAdapter.queryPdfReferrers(requestShape);
    return finalizeAdapterResult(adapterResult, mapPdfReferrers, requestId, requestShape);
  }

  async queryChemSpectrumMetadata(payload = {}) {
    const requestId = sanitizeRequestId(payload);
    const attachmentRelPath = ensureNonEmptyString(payload.attachmentRelPath, "attachmentRelPath", requestId);
    if (typeof attachmentRelPath !== "string") {
      return attachmentRelPath;
    }

    const limit = ensureLimit(payload.limit, requestId);
    if (typeof limit !== "number") {
      return limit;
    }

    const requestShape = { attachmentRelPath, limit };
    const adapterResult = await this.kernelAdapter.queryChemSpectrumMetadata(requestShape);
    return finalizeAdapterResult(adapterResult, mapDomainMetadataList, requestId, requestShape);
  }

  async listChemSpectra(payload = {}) {
    const requestId = sanitizeRequestId(payload);
    const limit = ensureLimit(payload.limit, requestId);
    if (typeof limit !== "number") {
      return limit;
    }

    const adapterResult = await this.kernelAdapter.listChemSpectra({ limit });
    return finalizeAdapterResult(adapterResult, mapChemSpectrumList, requestId);
  }

  async getChemSpectrum(payload = {}) {
    const requestId = sanitizeRequestId(payload);
    const attachmentRelPath = ensureNonEmptyString(payload.attachmentRelPath, "attachmentRelPath", requestId);
    if (typeof attachmentRelPath !== "string") {
      return attachmentRelPath;
    }

    const adapterResult = await this.kernelAdapter.getChemSpectrum({ attachmentRelPath });
    return finalizeAdapterResult(adapterResult, mapChemSpectrumRecord, requestId);
  }

  async queryNoteChemSpectrumRefs(payload = {}) {
    const requestId = sanitizeRequestId(payload);
    const noteRelPath = ensureNonEmptyString(payload.noteRelPath, "noteRelPath", requestId);
    if (typeof noteRelPath !== "string") {
      return noteRelPath;
    }

    const limit = ensureLimit(payload.limit, requestId);
    if (typeof limit !== "number") {
      return limit;
    }

    const requestShape = { noteRelPath, limit };
    const adapterResult = await this.kernelAdapter.queryNoteChemSpectrumRefs(requestShape);
    return finalizeAdapterResult(adapterResult, mapChemSpectrumSourceRefs, requestId, requestShape);
  }

  async queryChemSpectrumReferrers(payload = {}) {
    const requestId = sanitizeRequestId(payload);
    const attachmentRelPath = ensureNonEmptyString(payload.attachmentRelPath, "attachmentRelPath", requestId);
    if (typeof attachmentRelPath !== "string") {
      return attachmentRelPath;
    }

    const limit = ensureLimit(payload.limit, requestId);
    if (typeof limit !== "number") {
      return limit;
    }

    const requestShape = { attachmentRelPath, limit };
    const adapterResult = await this.kernelAdapter.queryChemSpectrumReferrers(requestShape);
    return finalizeAdapterResult(adapterResult, mapChemSpectrumReferrers, requestId, requestShape);
  }

  async exportSupportBundle(payload = {}) {
    const requestId = sanitizeRequestId(payload);
    const outputPath = ensureNonEmptyString(payload.outputPath, "outputPath", requestId);
    if (typeof outputPath !== "string") {
      return outputPath;
    }

    const requestShape = { outputPath };
    const adapterResult = await this.kernelAdapter.exportDiagnostics(requestShape);
    return finalizeAdapterResult(adapterResult, mapDiagnosticsExport, requestId, requestShape);
  }

  async getRebuildStatus(payload = {}) {
    const requestId = sanitizeRequestId(payload);
    return ok(
      {
        runMode: getRunMode(),
        adapterAttached: this.kernelAdapter.getBindingInfo().attached,
        status: mapKernelRebuildStatus(this.kernelAdapter.getRebuildStatusSummary())
      },
      requestId
    );
  }

  async startRebuild(payload = {}) {
    const requestId = sanitizeRequestId(payload);
    const adapterResult = await this.kernelAdapter.startRebuild();
    return finalizeAdapterResult(
      adapterResult,
      (value) => ({
        result: value.result ?? "started"
      }),
      requestId
    );
  }

  async waitForRebuild(payload = {}) {
    const requestId = sanitizeRequestId(payload);
    const timeoutMs = payload.timeoutMs == null ? REBUILD_WAIT_DEFAULT_MS : payload.timeoutMs;
    if (!Number.isInteger(timeoutMs) || timeoutMs < 0) {
      return fail(
        HOST_ERROR_CODES.invalidArgument,
        "timeoutMs must be a non-negative integer.",
        {
          field: "timeoutMs"
        },
        requestId
      );
    }

    const adapterResult = await this.kernelAdapter.waitForRebuild({ timeoutMs });
    return finalizeAdapterResult(
      adapterResult,
      (value) => ({
        timeoutMs,
        result: value.result ?? "completed"
      }),
      requestId
    );
  }
}

function ensureOptionalString(value, fieldName, requestId) {
  if (typeof value !== "string") {
    return fail(
      HOST_ERROR_CODES.invalidArgument,
      `${fieldName} must be a string when provided.`,
      {
        field: fieldName
      },
      requestId
    );
  }

  return value.trim();
}

function mapFilesSurfaceFailure(error, requestId) {
  if (error instanceof FilesSurfaceError) {
    return fail(
      error.code,
      error.message,
      error.details,
      requestId
    );
  }

  return fail(
    HOST_ERROR_CODES.internalError,
    "Files surface failed unexpectedly.",
    null,
    requestId
  );
}

module.exports = {
  HostApi
};
