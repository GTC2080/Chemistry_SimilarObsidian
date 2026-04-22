/**
 * host-api-client.js
 *
 * Thin wrapper over window.hostShell.*.
 * This is the ONLY file in the renderer allowed to call window.hostShell.
 * All other modules import from here.
 */

import { sanitizeEnvelope } from "./envelope-guard.js";

function guardShell() {
  if (!window.hostShell) {
    return {
      ok: false,
      data: null,
      error: {
        code: "HOST_KERNEL_ADAPTER_UNAVAILABLE",
        message: "window.hostShell is not available. Preload bridge missing.",
        details: null
      }
    };
  }
  return null;
}

async function invoke(group, method, payload = {}, requestId) {
  const shellCheck = guardShell();
  if (shellCheck) {
    return shellCheck;
  }

  const apiGroup = window.hostShell[group];
  if (!apiGroup || typeof apiGroup[method] !== "function") {
    return {
      ok: false,
      data: null,
      error: {
        code: "RENDERER_API_NOT_FOUND",
        message: `Host API group '${group}.${method}' is not exposed.`,
        details: { group, method }
      }
    };
  }

  try {
    const raw = await apiGroup[method](payload, requestId);
    return sanitizeEnvelope(raw);
  } catch (err) {
    return {
      ok: false,
      data: null,
      error: {
        code: "RENDERER_INVOCATION_EXCEPTION",
        message: err && err.message ? err.message : String(err),
        details: null
      }
    };
  }
}

export const bootstrap = {
  async getInfo(requestId) {
    return invoke("bootstrap", "getInfo", {}, requestId);
  }
};

export const runtime = {
  async getSummary(requestId) {
    return invoke("runtime", "getSummary", {}, requestId);
  }
};

export const session = {
  async getStatus(requestId) {
    return invoke("session", "getStatus", {}, requestId);
  },

  async openVault(vaultPath, requestId) {
    return invoke("session", "openVault", { vaultPath }, requestId);
  },

  async closeVault(requestId) {
    return invoke("session", "closeVault", {}, requestId);
  }
};

export const search = {
  async query(request = {}, requestId) {
    return invoke("search", "query", request, requestId);
  }
};

export const attachments = {
  async list(request = {}, requestId) {
    return invoke("attachments", "list", request, requestId);
  },

  async get(request = {}, requestId) {
    return invoke("attachments", "get", request, requestId);
  },

  async queryNoteRefs(request = {}, requestId) {
    return invoke("attachments", "queryNoteRefs", request, requestId);
  },

  async queryReferrers(request = {}, requestId) {
    return invoke("attachments", "queryReferrers", request, requestId);
  }
};

export const pdf = {
  async getMetadata(request = {}, requestId) {
    return invoke("pdf", "getMetadata", request, requestId);
  },

  async queryNoteSourceRefs(request = {}, requestId) {
    return invoke("pdf", "queryNoteSourceRefs", request, requestId);
  },

  async queryReferrers(request = {}, requestId) {
    return invoke("pdf", "queryReferrers", request, requestId);
  }
};

export const chemistry = {
  async queryMetadata(request = {}, requestId) {
    return invoke("chemistry", "queryMetadata", request, requestId);
  },

  async listSpectra(request = {}, requestId) {
    return invoke("chemistry", "listSpectra", request, requestId);
  },

  async getSpectrum(request = {}, requestId) {
    return invoke("chemistry", "getSpectrum", request, requestId);
  },

  async queryNoteRefs(request = {}, requestId) {
    return invoke("chemistry", "queryNoteRefs", request, requestId);
  },

  async queryReferrers(request = {}, requestId) {
    return invoke("chemistry", "queryReferrers", request, requestId);
  }
};

export const diagnostics = {
  async exportSupportBundle(request = {}, requestId) {
    return invoke("diagnostics", "exportSupportBundle", request, requestId);
  }
};

export const rebuild = {
  async getStatus(requestId) {
    return invoke("rebuild", "getStatus", {}, requestId);
  },

  async start(requestId) {
    return invoke("rebuild", "start", {}, requestId);
  },

  async wait(request = {}, requestId) {
    return invoke("rebuild", "wait", request, requestId);
  }
};
