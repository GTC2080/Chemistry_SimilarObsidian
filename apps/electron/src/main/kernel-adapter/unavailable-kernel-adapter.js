const path = require("node:path");
const {
  HOST_ERROR_CODES,
  HOST_INDEX_STATES
} = require("../../shared/host-contract");

function adapterOk(value) {
  return {
    ok: true,
    value,
    error: null
  };
}

function adapterFail(operation, details = null, code = HOST_ERROR_CODES.kernelAdapterUnavailable, message = "Kernel adapter is not attached yet.") {
  return {
    ok: false,
    value: null,
    error: {
      code,
      message,
      details: {
        adapter_state: details && details.adapter_state ? details.adapter_state : "unavailable",
        operation,
        ...(details ?? {})
      }
    }
  };
}

function createUnavailableKernelAdapter(options = {}) {
  const adapterState = options.adapterState ?? "unavailable";
  const failureCode = options.failureCode ?? HOST_ERROR_CODES.kernelAdapterUnavailable;
  const failureMessage = options.failureMessage ?? "Kernel adapter is not attached yet.";
  const bindingMechanism = options.bindingMechanism ?? null;
  const runMode = options.runMode ?? null;
  const bindingPath = options.bindingPath ?? null;
  const bindingCandidates = options.bindingCandidates ?? null;
  const failureDetails = options.failureDetails ?? null;

  function failFor(operation, extraDetails = null) {
    return adapterFail(
      operation,
      {
        adapter_state: adapterState,
        binding_mechanism: bindingMechanism,
        run_mode: runMode,
        binding_path: bindingPath,
        binding_candidates: bindingCandidates,
        ...(failureDetails ?? {}),
        ...(extraDetails ?? {})
      },
      failureCode,
      failureMessage
    );
  }

  return {
    getBindingInfo() {
      return {
        adapter_state: adapterState,
        attached: false,
        native_binary_mode: runMode,
        native_binary_path: bindingPath,
        native_binary_loaded: false,
        binding_mechanism: bindingMechanism,
        binding_candidates: bindingCandidates,
        failure_code: failureCode
      };
    },

    getKernelRuntimeSummary() {
      return {
        session_state: "closed",
        index_state: HOST_INDEX_STATES.unavailable,
        indexed_note_count: 0,
        pending_recovery_ops: 0
      };
    },

    getRebuildStatusSummary() {
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
    },

    async openVault() {
      return failFor("session.open_vault");
    },

    async closeVault() {
      return adapterOk({
        result: "already_closed"
      });
    },

    async querySearch() {
      return failFor("search.query");
    },

    async listAttachments() {
      return failFor("attachments.list");
    },

    async getAttachment() {
      return failFor("attachments.get");
    },

    async queryNoteAttachmentRefs() {
      return failFor("attachments.query_note_refs");
    },

    async queryAttachmentReferrers() {
      return failFor("attachments.query_referrers");
    },

    async getPdfMetadata() {
      return failFor("pdf.get_metadata");
    },

    async queryNotePdfSourceRefs() {
      return failFor("pdf.query_note_source_refs");
    },

    async queryPdfReferrers() {
      return failFor("pdf.query_referrers");
    },

    async queryChemSpectrumMetadata() {
      return failFor("chemistry.query_metadata");
    },

    async listChemSpectra() {
      return failFor("chemistry.list_spectra");
    },

    async getChemSpectrum() {
      return failFor("chemistry.get_spectrum");
    },

    async queryNoteChemSpectrumRefs() {
      return failFor("chemistry.query_note_refs");
    },

    async queryChemSpectrumReferrers() {
      return failFor("chemistry.query_referrers");
    },

    async exportDiagnostics(payload = {}) {
      return failFor("diagnostics.export_support_bundle", {
        output_path: payload.outputPath ? path.resolve(payload.outputPath) : null
      });
    },

    async startRebuild() {
      return failFor("rebuild.start");
    },

    async waitForRebuild() {
      return failFor("rebuild.wait");
    }
  };
}

module.exports = {
  adapterFail,
  adapterOk,
  createUnavailableKernelAdapter
};
