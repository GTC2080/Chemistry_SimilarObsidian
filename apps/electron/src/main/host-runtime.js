const path = require("node:path");
const { app } = require("electron");
const { createKernelAdapterBoundary } = require("./kernel-adapter/kernel-adapter-boundary");
const {
  HOST_ERROR_CODES,
  HOST_LIFECYCLE_STATES,
  HOST_SESSION_STATES,
  SECURITY_BASELINE
} = require("../shared/host-contract");

function appendHostDebug(label, payload = null) {
  try {
    const debugPath = path.join(app.getPath("userData"), "host-debug.log");
    const line = JSON.stringify({
      at_ms: Date.now(),
      label,
      ...(payload ?? {})
    });
    require("node:fs").appendFileSync(debugPath, `${line}\n`, "utf8");
  } catch {
    // Best-effort debug trace only.
  }
}

function ok(data, requestId) {
  const envelope = {
    ok: true,
    data,
    error: null
  };

  if (requestId) {
    envelope.request_id = requestId;
  }

  return envelope;
}

function fail(code, message, details, requestId) {
  const envelope = {
    ok: false,
    data: null,
    error: {
      code,
      message,
      details: details ?? null
    }
  };

  if (requestId) {
    envelope.request_id = requestId;
  }

  return envelope;
}

function sanitizeRequestId(payload) {
  return payload && typeof payload.request_id === "string" && payload.request_id.trim()
    ? payload.request_id.trim()
    : null;
}

function normalizeVaultPath(vaultPath) {
  return path.normalize(path.resolve(vaultPath));
}

function sameVaultPath(left, right) {
  if (process.platform === "win32") {
    return left.toLowerCase() === right.toLowerCase();
  }

  return left === right;
}

class HostRuntime {
  constructor(options = {}) {
    this.kernelAdapter = options.kernelAdapter ?? createKernelAdapterBoundary();
    this.lifecycleState = HOST_LIFECYCLE_STATES.booting;
    this.sessionState = HOST_SESSION_STATES.none;
    this.activeSession = null;
    this.lastSessionError = null;
    this.lastWindowEvent = null;
    this.mainWindow = null;
  }

  bindMainWindow(mainWindow) {
    this.mainWindow = mainWindow;
  }

  markReady() {
    this.lifecycleState = HOST_LIFECYCLE_STATES.ready;
  }

  async prepareForShutdown() {
    this.lifecycleState = HOST_LIFECYCLE_STATES.shuttingDown;

    if (this.activeSession) {
      await this.closeVault({
        shutdown: true
      });
    }

    this.lifecycleState = HOST_LIFECYCLE_STATES.closed;
  }

  noteWindowEvent(kind, details = null) {
    this.lastWindowEvent = {
      kind,
      details,
      at_ms: Date.now()
    };
  }

  getBootstrapInfo() {
    return {
      shell: "electron-host-baseline",
      host_version: app.getVersion(),
      run_mode: app.isPackaged ? "packaged" : "dev",
      packaged: app.isPackaged,
      platform: process.platform,
      versions: {
        electron: process.versions.electron,
        chrome: process.versions.chrome,
        node: process.versions.node
      },
      security: SECURITY_BASELINE,
      api_groups: [
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
      ],
      renderer_boundary: {
        direct_node_access: false,
        direct_electron_access: false,
        preload_only_bridge: true
      }
    };
  }

  getRuntimeSummary() {
    return {
      lifecycle_state: this.lifecycleState,
      run_mode: app.isPackaged ? "packaged" : "dev",
      main_window: {
        exists: Boolean(this.mainWindow && !this.mainWindow.isDestroyed()),
        visible: Boolean(
          this.mainWindow &&
          !this.mainWindow.isDestroyed() &&
          this.mainWindow.isVisible()
        )
      },
      kernel_runtime: this.kernelAdapter.getKernelRuntimeSummary(),
      rebuild: this.kernelAdapter.getRebuildStatusSummary(),
      session: this.getSessionData(),
      kernel_binding: this.kernelAdapter.getBindingInfo(),
      last_window_event: this.lastWindowEvent
    };
  }

  getSessionData() {
    return {
      state: this.sessionState,
      active_vault_path: this.activeSession ? this.activeSession.vaultPath : null,
      adapter_attached: this.kernelAdapter.getBindingInfo().attached,
      last_error: this.lastSessionError
    };
  }

  getActiveVaultPath() {
    return this.activeSession ? this.activeSession.vaultPath : null;
  }

  async getSessionStatus(payload = {}) {
    return ok(this.getSessionData(), sanitizeRequestId(payload));
  }

  async openVault(payload = {}) {
    const requestId = sanitizeRequestId(payload);
    const vaultPath = payload && typeof payload.vaultPath === "string"
      ? payload.vaultPath.trim()
      : "";

    appendHostDebug("host_runtime.open_vault.begin", {
      requestId,
      vaultPath
    });

    if (!vaultPath) {
      return fail(
        HOST_ERROR_CODES.invalidArgument,
        "vaultPath must be a non-empty string.",
        {
          field: "vaultPath"
        },
        requestId
      );
    }

    if (this.sessionState === HOST_SESSION_STATES.opening || this.sessionState === HOST_SESSION_STATES.closing) {
      return fail(
        HOST_ERROR_CODES.busy,
        "A vault session transition is already in progress.",
        {
          session_state: this.sessionState
        },
        requestId
      );
    }

    const normalizedVaultPath = normalizeVaultPath(vaultPath);

    if (this.activeSession && sameVaultPath(this.activeSession.vaultPath, normalizedVaultPath)) {
      return ok(
        {
          result: "already_open",
          session: this.getSessionData()
        },
        requestId
      );
    }

    if (this.activeSession) {
      const closeResult = await this.closeVault({
        request_id: requestId
      });

      if (!closeResult.ok) {
        return closeResult;
      }
    }

    this.sessionState = HOST_SESSION_STATES.opening;
    this.lastSessionError = null;

    const openResult = await this.kernelAdapter.openVault({
      vaultPath: normalizedVaultPath,
      runMode: app.isPackaged ? "packaged" : "dev"
    });

    appendHostDebug("host_runtime.open_vault.adapter_result", {
      requestId,
      normalizedVaultPath,
      ok: openResult.ok,
      errorCode: openResult.ok ? null : openResult.error.code
    });

    if (!openResult.ok) {
      this.sessionState = HOST_SESSION_STATES.none;
      this.activeSession = null;
      this.lastSessionError = {
        code: openResult.error.code,
        message: openResult.error.message,
        details: openResult.error.details,
        at_ms: Date.now()
      };
      return fail(
        openResult.error.code,
        openResult.error.message,
        openResult.error.details,
        requestId
      );
    }

    this.activeSession = {
      vaultPath: normalizedVaultPath,
      openedAtMs: Date.now()
    };
    this.sessionState = HOST_SESSION_STATES.open;

    appendHostDebug("host_runtime.open_vault.done", {
      requestId,
      normalizedVaultPath
    });

    return ok(
      {
        result: "opened",
        session: this.getSessionData()
      },
      requestId
    );
  }

  async closeVault(payload = {}) {
    const requestId = sanitizeRequestId(payload);

    if (this.sessionState === HOST_SESSION_STATES.opening || this.sessionState === HOST_SESSION_STATES.closing) {
      return fail(
        HOST_ERROR_CODES.busy,
        "A vault session transition is already in progress.",
        {
          session_state: this.sessionState
        },
        requestId
      );
    }

    if (!this.activeSession) {
      this.sessionState = HOST_SESSION_STATES.none;
      return ok(
        {
          result: "already_closed",
          session: this.getSessionData()
        },
        requestId
      );
    }

    this.sessionState = HOST_SESSION_STATES.closing;

    const closeResult = await this.kernelAdapter.closeVault({
      vaultPath: this.activeSession.vaultPath,
      shutdown: Boolean(payload.shutdown)
    });

    if (!closeResult.ok) {
      this.sessionState = HOST_SESSION_STATES.open;
      this.lastSessionError = {
        code: closeResult.error.code,
        message: closeResult.error.message,
        details: closeResult.error.details,
        at_ms: Date.now()
      };
      return fail(
        closeResult.error.code,
        closeResult.error.message,
        closeResult.error.details,
        requestId
      );
    }

    this.activeSession = null;
    this.sessionState = HOST_SESSION_STATES.none;

    return ok(
      {
        result: "closed",
        session: this.getSessionData()
      },
      requestId
    );
  }
}

module.exports = {
  HostRuntime,
  ok,
  fail
};
