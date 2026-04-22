/**
 * app-shell.js
 *
 * Root module: routes between Launcher (no vault) and Workspace (vault open).
 */

import { store } from "./state/host-store.js";
import { bootstrap, runtime, session } from "./services/host-api-client.js";
import { createStateSurface } from "./components/shared/state-surface.js";
import { createRuntimeStatusBadge } from "./components/shared/runtime-status-badge.js";
import { createLauncherShell } from "./components/layout/launcher-shell.js";
import { createLauncherPage } from "./pages/launcher/launcher-page.js";
import { createWorkspaceShell } from "./components/layout/workspace-shell.js";
import { createWorkspaceHomePage } from "./pages/workspace/workspace-home-page.js";
import { createSearchPage } from "./pages/search/search-page.js";
import { createAttachmentPage } from "./pages/attachments/attachment-page.js";
import { createChemistryPage } from "./pages/chemistry/chemistry-page.js";
import { createDiagnosticsPage } from "./pages/diagnostics/diagnostics-page.js";

const POLLING_INTERVAL_MS = 5000;

class AppShell {
  constructor() {
    this._root = null;
    this._runtimeTimer = null;
    this._initialized = false;
    this._sessionTransitioning = false;
    this._lastSessionOpen = undefined;
    this._visibilityHandler = null;
  }

  async init() {
    if (this._initialized) return;
    this._initialized = true;

    this._root = document.getElementById("app-root");

    // 1. Bootstrap check
    const bootstrapEnv = await bootstrap.getInfo("app-init");
    store.setEnvelope("bootstrap", bootstrapEnv);

    if (!bootstrapEnv.ok) {
      store.setHostAvailable(false);
      this._renderUnavailable("Bootstrap failed. Host bridge is not reachable.");
      return;
    }

    store.setHostAvailable(true);

    // 2. Initial session + runtime fetch
    const [runtimeEnv, sessionEnv] = await Promise.all([
      runtime.getSummary("app-init-runtime"),
      session.getStatus("app-init-session")
    ]);
    store.setEnvelope("runtime", runtimeEnv);
    store.setEnvelope("session", sessionEnv);

    // 3. Determine mode based on session state
    const sessionState = store.getSessionState();
    const initialPage = sessionState === "open" ? "files" : "launcher";
    store.setCurrentPage(initialPage);

    // 4. Subscribe to store changes
    store.subscribe(() => this._onStoreUpdate());

    // 5. Start runtime polling if session is open
    if (sessionState === "open") {
      this._startRuntimePolling();
    }

    // 6. Render initial mode
    this._renderMode();

    // 7. Visibility change handling
    this._visibilityHandler = () => {
      if (document.visibilityState === "visible") {
        if (store.getSessionState() === "open") {
          runtime.getSummary("app-visibility-resume").then((env) => store.setEnvelope("runtime", env));
        }
      }
    };
    document.addEventListener("visibilitychange", this._visibilityHandler);

    // 8. Expose smoke helpers
    this._exposeSmokeHelpers();
  }

  _onStoreUpdate() {
    const sessionState = store.getSessionState();
    const sessionOpen = sessionState === "open";
    const snapshot = store.snapshot();

    // Handle session transition: start/stop polling and page routing
    const prevSessionOpen = this._lastSessionOpen;
    this._lastSessionOpen = sessionOpen;

    if (prevSessionOpen !== sessionOpen) {
      if (sessionOpen) {
        this._startRuntimePolling();
        if (snapshot.currentPage === "launcher") {
          store.setCurrentPage("files");
        }
      } else {
        this._stopRuntimePolling();
        store.setCurrentPage("launcher");
      }
    }

    this._renderMode();
  }

  _renderMode() {
    if (!this._root) return;

    const snapshot = store.snapshot();

    // Host unavailable takes absolute precedence
    if (!snapshot.hostAvailable) {
      this._root.innerHTML = "";
      const surface = createStateSurface("unavailable", {
        message: "Host unavailable. Preload bridge may be missing.",
        onRetry: () => window.location.reload()
      });
      this._root.appendChild(surface);
      return;
    }

    const currentPage = snapshot.currentPage;
    const sessionState = store.getSessionState();

    if (currentPage === "launcher" || sessionState !== "open") {
      this._renderLauncher();
      return;
    }

    // Workspace mode: vault is open
    this._renderWorkspace();
  }

  _renderLauncher() {
    if (!this._root) return;
    this._root.innerHTML = "";

    const runtimeEnv = store.getRuntimeSummary();
    const badge = createRuntimeStatusBadge(runtimeEnv);

    const { element, card } = createLauncherShell({ statusBadge: badge });

    const launcherPage = createLauncherPage({
      onOpenVault: (path) => this._openVault(path),
      lastError: store.getLastSessionError(),
      isOpening: this._sessionTransitioning
    });

    card.appendChild(launcherPage);
    this._root.appendChild(element);
  }

  _renderWorkspace() {
    if (!this._root) return;
    this._root.innerHTML = "";

    const currentPage = store.getCurrentPage();

    const content = this._buildWorkspaceContent(currentPage);

    const { element } = createWorkspaceShell({
      currentPage,
      vaultName: store.getActiveVaultPath() || "Vault",
      runtimeEnvelope: store.getRuntimeSummary(),
      onNavigate: (pageId) => {
        store.setCurrentPage(pageId);
        this._renderWorkspace();
      },
      onCloseVault: () => this._closeVault(),
      children: content
    });

    this._root.appendChild(element);
  }

  _buildWorkspaceContent(currentPage) {
    const container = document.createElement("div");

    // Degraded banner
    const banner = this._buildDegradedBanner();
    if (banner) container.appendChild(banner);

    if (currentPage === "search") {
      container.appendChild(createSearchPage());
    } else if (currentPage === "attachments") {
      container.appendChild(createAttachmentPage());
    } else if (currentPage === "chemistry") {
      container.appendChild(createChemistryPage());
    } else if (currentPage === "diagnostics") {
      container.appendChild(createDiagnosticsPage());
    } else {
      container.appendChild(createWorkspaceHomePage({ vaultPath: store.getActiveVaultPath() }));
    }

    return container;
  }

  _buildDegradedBanner() {
    const runtimeEnv = store.getRuntimeSummary();
    if (!runtimeEnv || !runtimeEnv.ok) return null;

    const indexState = runtimeEnv.data?.kernel_runtime?.index_state;
    const attached = runtimeEnv.data?.kernel_binding?.attached;
    const messages = [];

    if (!attached) {
      messages.push("Kernel adapter detached. Some features may be unavailable.");
    } else if (indexState === "catching_up") {
      messages.push("Index is catching up. Search results may be incomplete.");
    } else if (indexState === "rebuilding") {
      messages.push("Index is rebuilding. Search results may be incomplete.");
    }

    if (messages.length === 0) return null;

    const banner = document.createElement("div");
    banner.style.cssText = `
      padding: 10px 14px;
      margin-bottom: 12px;
      border-radius: 6px;
      background: #fffbeb;
      border: 1px solid #fcd34d;
      color: #92400e;
      font-size: 13px;
    `;
    banner.textContent = messages.join(" ");
    return banner;
  }

  _renderUnavailable(message) {
    const root = document.getElementById("app-root");
    if (!root) return;
    root.innerHTML = "";
    const surface = createStateSurface("unavailable", {
      message,
      onRetry: () => window.location.reload()
    });
    root.appendChild(surface);
  }

  async _openVault(vaultPath) {
    this._sessionTransitioning = true;
    this._renderMode();

    const result = await session.openVault(vaultPath, "app-open-vault");
    this._sessionTransitioning = false;
    store.setEnvelope("session", result);

    if (result.ok) {
      const runtimeEnv = await runtime.getSummary("app-post-open");
      store.setEnvelope("runtime", runtimeEnv);
    }
  }

  async _closeVault() {
    this._sessionTransitioning = true;
    this._renderMode();

    const result = await session.closeVault("app-close-vault");
    this._sessionTransitioning = false;
    store.setEnvelope("session", result);

    if (result.ok) {
      store.setEnvelope("runtime", null);
    }
  }

  _startRuntimePolling() {
    this._stopRuntimePolling();

    const tick = async () => {
      if (document.visibilityState === "hidden") return;
      const env = await runtime.getSummary("app-poll");
      store.setEnvelope("runtime", env);
    };

    tick();
    this._runtimeTimer = setInterval(tick, POLLING_INTERVAL_MS);
  }

  _stopRuntimePolling() {
    if (this._runtimeTimer) {
      clearInterval(this._runtimeTimer);
      this._runtimeTimer = null;
    }
  }

  _exposeSmokeHelpers() {
    window.__rendererSmoke = {
      getPageName: () => store.getCurrentPage(),
      navigateTo: (pageName) => {
        store.setCurrentPage(pageName);
        this._renderMode();
      },
      getHostStoreSnapshot: () => store.snapshot()
    };
  }
}

export const appShell = new AppShell();
