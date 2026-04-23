const {
  adapterFail,
  adapterOk,
  createUnavailableKernelAdapter
} = require("./unavailable-kernel-adapter");
const { tryLoadNativeBinding } = require("./native-binding-loader");
const { HOST_ERROR_CODES, HOST_INDEX_STATES } = require("../../shared/host-contract");
const fs = require("node:fs");
const path = require("node:path");

function appendHostDebug(label, payload = null) {
  try {
    const appData = process.env.APPDATA;
    if (!appData) {
      return;
    }

    const debugPath = path.join(appData, "chemistry-obsidian-electron-host", "host-debug.log");
    const line = JSON.stringify({
      at_ms: Date.now(),
      label,
      ...(payload ?? {})
    });
    fs.appendFileSync(debugPath, `${line}\n`, "utf8");
  } catch {
    // Best-effort debug trace only.
  }
}

const KERNEL_SESSION_STATE_NAMES = Object.freeze({
  0: "closed",
  1: "open",
  2: "faulted"
});

const KERNEL_INDEX_STATE_NAMES = Object.freeze({
  0: HOST_INDEX_STATES.unavailable,
  1: HOST_INDEX_STATES.catchingUp,
  2: HOST_INDEX_STATES.ready,
  3: HOST_INDEX_STATES.rebuilding
});

const NATIVE_ERROR_MAP = Object.freeze({
  KERNEL_ERROR_INVALID_ARGUMENT: {
    code: HOST_ERROR_CODES.invalidArgument,
    message: "Kernel rejected the request arguments."
  },
  KERNEL_ERROR_NOT_FOUND: {
    code: HOST_ERROR_CODES.kernelNotFound,
    message: "Kernel target was not found."
  },
  KERNEL_ERROR_CONFLICT: {
    code: HOST_ERROR_CODES.kernelConflict,
    message: "Kernel reported a conflict for the requested operation."
  },
  KERNEL_ERROR_IO: {
    code: HOST_ERROR_CODES.kernelIoError,
    message: "Kernel reported an I/O failure."
  },
  KERNEL_ERROR_INTERNAL: {
    code: HOST_ERROR_CODES.internalError,
    message: "Kernel reported an internal failure."
  },
  KERNEL_ERROR_TIMEOUT: {
    code: HOST_ERROR_CODES.kernelTimeout,
    message: "Kernel operation timed out."
  },
  BINDING_INVALID_ARGUMENT: {
    code: HOST_ERROR_CODES.invalidArgument,
    message: "Kernel binding received invalid arguments."
  },
  BINDING_INVALID_SESSION_HANDLE: {
    code: HOST_ERROR_CODES.internalError,
    message: "Kernel session handle is invalid inside the host adapter."
  },
  BINDING_SESSION_CLOSED: {
    code: HOST_ERROR_CODES.internalError,
    message: "Kernel session handle is already closed inside the host adapter."
  },
  BINDING_ALLOCATION_FAILED: {
    code: HOST_ERROR_CODES.internalError,
    message: "Kernel binding failed to allocate a session wrapper."
  }
});

function makeDetachedRuntimeSummary() {
  return {
    session_state: "closed",
    index_state: HOST_INDEX_STATES.unavailable,
    indexed_note_count: 0,
    pending_recovery_ops: 0
  };
}

function makeDetachedRebuildSummary() {
  return {
    in_flight: false,
    has_last_result: false,
    current_generation: 0,
    last_completed_generation: 0,
    current_started_at_ns: 0,
    last_result_code: null,
    last_result_at_ns: 0,
    index_state: HOST_INDEX_STATES.unavailable
  };
}

function normalizeKernelStateSnapshot(snapshot = {}) {
  return {
    session_state: KERNEL_SESSION_STATE_NAMES[snapshot.sessionStateCode] ?? "unknown",
    index_state: KERNEL_INDEX_STATE_NAMES[snapshot.indexStateCode] ?? HOST_INDEX_STATES.unavailable,
    indexed_note_count: Number(snapshot.indexedNoteCount ?? 0),
    pending_recovery_ops: Number(snapshot.pendingRecoveryOps ?? 0)
  };
}

function normalizeRebuildStatus(snapshot = {}, indexState = HOST_INDEX_STATES.unavailable) {
  return {
    in_flight: Boolean(snapshot.inFlight),
    has_last_result: Boolean(snapshot.hasLastResult),
    current_generation: Number(snapshot.currentGeneration ?? 0),
    last_completed_generation: Number(snapshot.lastCompletedGeneration ?? 0),
    current_started_at_ns: Number(snapshot.currentStartedAtNs ?? 0),
    last_result_code: snapshot.lastResultCode ?? null,
    last_result_at_ns: Number(snapshot.lastResultAtNs ?? 0),
    index_state: indexState
  };
}

function surfaceNotIntegrated(bindingState, operation) {
  return adapterFail(
    operation,
    {
      adapter_state: "binding_loaded",
      binding_mechanism: bindingState.bindingMechanism,
      run_mode: bindingState.runMode
    },
    HOST_ERROR_CODES.kernelSurfaceNotIntegrated,
    "Kernel binding is loaded, but this host surface is not integrated yet."
  );
}

function sessionNotOpen(bindingState, operation) {
  return adapterFail(
    operation,
    {
      adapter_state: "binding_loaded",
      binding_mechanism: bindingState.bindingMechanism,
      run_mode: bindingState.runMode
    },
    HOST_ERROR_CODES.sessionNotOpen,
    "Open a vault session before using this host surface."
  );
}

function mapNativeBindingError(bindingState, operation, error, extraDetails = null) {
  const nativeCode = typeof error?.code === "string" ? error.code : null;
  let mapped = nativeCode && NATIVE_ERROR_MAP[nativeCode]
    ? NATIVE_ERROR_MAP[nativeCode]
    : {
      code: HOST_ERROR_CODES.internalError,
      message: "Kernel binding call failed."
    };

  if (operation === "rebuild.start" && nativeCode === "KERNEL_ERROR_CONFLICT") {
    mapped = {
      code: HOST_ERROR_CODES.rebuildAlreadyRunning,
      message: "A rebuild is already running."
    };
  } else if (operation === "rebuild.wait" && nativeCode === "KERNEL_ERROR_NOT_FOUND") {
    mapped = {
      code: HOST_ERROR_CODES.rebuildNotInFlight,
      message: "No rebuild is currently in flight."
    };
  }

  return {
    ok: false,
    value: null,
    error: {
      code: mapped.code,
      message: mapped.message,
      details: {
        adapter_state: "binding_loaded",
        operation,
        native_code: nativeCode,
        native_message: error instanceof Error ? error.message : String(error),
        binding_mechanism: bindingState.bindingMechanism,
        run_mode: bindingState.runMode,
        ...(typeof error?.kernelCode === "number" ? { kernel_code: error.kernelCode } : {}),
        ...(extraDetails ?? {})
      }
    }
  };
}

function createBoundKernelAdapter(bindingState) {
  let activeSession = null;

  function getBindingInfo() {
    return {
      adapter_state: "binding_loaded",
      attached: true,
      native_binary_mode: bindingState.runMode,
      native_binary_path: bindingState.resolvedPath,
      native_binary_loaded: true,
      binding_mechanism: bindingState.bindingMechanism,
      binding_name: bindingState.bindingInfo.bindingName ?? null,
      binding_candidates: bindingState.candidates,
      session_attached: activeSession !== null
    };
  }

  function getKernelRuntimeSummary() {
    if (!activeSession) {
      return makeDetachedRuntimeSummary();
    }

    try {
      return normalizeKernelStateSnapshot(
        bindingState.binding.getState(activeSession.nativeHandle)
      );
    } catch {
      return {
        session_state: "faulted",
        index_state: HOST_INDEX_STATES.unavailable,
        indexed_note_count: 0,
        pending_recovery_ops: 0
      };
    }
  }

  function getRebuildStatusSummary() {
    if (!activeSession) {
      return makeDetachedRebuildSummary();
    }

    try {
      const runtimeSnapshot = bindingState.binding.getState(activeSession.nativeHandle);
      const rebuildSnapshot = bindingState.binding.getRebuildStatus(activeSession.nativeHandle);
      return normalizeRebuildStatus(
        rebuildSnapshot,
        KERNEL_INDEX_STATE_NAMES[runtimeSnapshot.indexStateCode] ?? HOST_INDEX_STATES.unavailable
      );
    } catch {
      return makeDetachedRebuildSummary();
    }
  }

  function withActiveSession(operation, work) {
    if (!activeSession) {
      return sessionNotOpen(bindingState, operation);
    }

    try {
      return adapterOk(work(activeSession));
    } catch (error) {
      return mapNativeBindingError(bindingState, operation, error, {
        active_vault_path: activeSession.vaultPath
      });
    }
  }

  return {
    getBindingInfo,
    getKernelRuntimeSummary,
    getRebuildStatusSummary,

    async openVault(payload = {}) {
      appendHostDebug("kernel_adapter.open_vault.begin", {
        vaultPath: payload.vaultPath ?? null,
        runMode: payload.runMode ?? null,
        alreadyActive: activeSession !== null
      });

      if (activeSession) {
        return adapterFail(
          "session.open_vault",
          {
            adapter_state: "binding_loaded",
            active_vault_path: activeSession.vaultPath
          },
          HOST_ERROR_CODES.busy,
          "A kernel session is already open in the host adapter."
        );
      }

      try {
        appendHostDebug("kernel_adapter.open_vault.native_call.begin", {
          vaultPath: payload.vaultPath ?? null
        });
        const nativeHandle = bindingState.binding.openVault(payload.vaultPath);
        appendHostDebug("kernel_adapter.open_vault.native_call.done", {
          vaultPath: payload.vaultPath ?? null
        });
        activeSession = {
          nativeHandle,
          vaultPath: payload.vaultPath
        };
        appendHostDebug("kernel_adapter.open_vault.done", {
          vaultPath: payload.vaultPath ?? null
        });
        return adapterOk({
          result: "opened"
        });
      } catch (error) {
        appendHostDebug("kernel_adapter.open_vault.error", {
          vaultPath: payload.vaultPath ?? null,
          error: error instanceof Error ? error.stack : String(error)
        });
        return mapNativeBindingError(bindingState, "session.open_vault", error, {
          vault_path: payload.vaultPath ?? null
        });
      }
    },

    async closeVault(payload = {}) {
      if (!activeSession) {
        return adapterOk({
          result: "already_closed"
        });
      }

      try {
        bindingState.binding.closeVault(activeSession.nativeHandle);
        activeSession = null;
        return adapterOk({
          result: payload.shutdown ? "closed_for_shutdown" : "closed"
        });
      } catch (error) {
        return mapNativeBindingError(bindingState, "session.close_vault", error, {
          vault_path: activeSession.vaultPath
        });
      }
    },

    async querySearch(request = {}) {
      return withActiveSession("search.query", ({ nativeHandle }) =>
        bindingState.binding.querySearch(nativeHandle, request)
      );
    },

    async listAttachments(request = {}) {
      return withActiveSession("attachments.list", ({ nativeHandle }) =>
        bindingState.binding.queryAttachments(nativeHandle, request.limit)
      );
    },

    async getAttachment(request = {}) {
      return withActiveSession("attachments.get", ({ nativeHandle }) =>
        bindingState.binding.getAttachment(nativeHandle, request.attachmentRelPath)
      );
    },

    async queryNoteAttachmentRefs(request = {}) {
      return withActiveSession("attachments.query_note_refs", ({ nativeHandle }) =>
        bindingState.binding.queryNoteAttachmentRefs(
          nativeHandle,
          request.noteRelPath,
          request.limit
        )
      );
    },

    async queryAttachmentReferrers(request = {}) {
      return withActiveSession("attachments.query_referrers", ({ nativeHandle }) =>
        bindingState.binding.queryAttachmentReferrers(
          nativeHandle,
          request.attachmentRelPath,
          request.limit
        )
      );
    },

    async getPdfMetadata(request = {}) {
      return withActiveSession("pdf.get_metadata", ({ nativeHandle }) =>
        bindingState.binding.getPdfMetadata(nativeHandle, request.attachmentRelPath)
      );
    },

    async queryNotePdfSourceRefs(request = {}) {
      return withActiveSession("pdf.query_note_source_refs", ({ nativeHandle }) =>
        bindingState.binding.queryNotePdfSourceRefs(
          nativeHandle,
          request.noteRelPath,
          request.limit
        )
      );
    },

    async queryPdfReferrers(request = {}) {
      return withActiveSession("pdf.query_referrers", ({ nativeHandle }) =>
        bindingState.binding.queryPdfReferrers(
          nativeHandle,
          request.attachmentRelPath,
          request.limit
        )
      );
    },

    async queryChemSpectrumMetadata(request = {}) {
      return withActiveSession("chemistry.query_metadata", ({ nativeHandle }) =>
        bindingState.binding.queryChemSpectrumMetadata(
          nativeHandle,
          request.attachmentRelPath,
          request.limit
        )
      );
    },

    async listChemSpectra(request = {}) {
      return withActiveSession("chemistry.list_spectra", ({ nativeHandle }) =>
        bindingState.binding.queryChemSpectra(nativeHandle, request.limit)
      );
    },

    async getChemSpectrum(request = {}) {
      return withActiveSession("chemistry.get_spectrum", ({ nativeHandle }) =>
        bindingState.binding.getChemSpectrum(nativeHandle, request.attachmentRelPath)
      );
    },

    async queryNoteChemSpectrumRefs(request = {}) {
      return withActiveSession("chemistry.query_note_refs", ({ nativeHandle }) =>
        bindingState.binding.queryNoteChemSpectrumRefs(
          nativeHandle,
          request.noteRelPath,
          request.limit
        )
      );
    },

    async queryChemSpectrumReferrers(request = {}) {
      return withActiveSession("chemistry.query_referrers", ({ nativeHandle }) =>
        bindingState.binding.queryChemSpectrumReferrers(
          nativeHandle,
          request.attachmentRelPath,
          request.limit
        )
      );
    },

    async exportDiagnostics(request = {}) {
      return withActiveSession("diagnostics.export_support_bundle", ({ nativeHandle }) =>
        bindingState.binding.exportDiagnostics(nativeHandle, request.outputPath)
      );
    },

    async startRebuild() {
      return withActiveSession("rebuild.start", ({ nativeHandle }) =>
        bindingState.binding.startRebuild(nativeHandle)
      );
    },

    async waitForRebuild(request = {}) {
      return withActiveSession("rebuild.wait", ({ nativeHandle }) =>
        bindingState.binding.waitForRebuild(nativeHandle, request.timeoutMs)
      );
    }
  };
}

function createKernelAdapterBoundary() {
  const bindingState = tryLoadNativeBinding();

  if (!bindingState.ok) {
    return createUnavailableKernelAdapter({
      adapterState: bindingState.loadState,
      failureCode: bindingState.error.code,
      failureMessage: bindingState.error.message,
      bindingMechanism: bindingState.bindingMechanism,
      runMode: bindingState.runMode,
      bindingPath: bindingState.resolvedPath,
      bindingCandidates: bindingState.candidates,
      failureDetails: bindingState.error.details ?? null
    });
  }

  return createBoundKernelAdapter(bindingState);
}

module.exports = {
  createKernelAdapterBoundary
};
