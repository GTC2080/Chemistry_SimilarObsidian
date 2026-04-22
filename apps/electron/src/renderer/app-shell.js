/**
 * app-shell.js
 *
 * Root module: bootstrap, routing, global state coordination.
 */

import { store } from "./state/host-store.js";
import { bootstrap, runtime, session } from "./services/host-api-client.js";
import { createAppLayout } from "./components/layout/app-layout.js";
import { createStateSurface } from "./components/shared/state-surface.js";
import { createRuntimeStatusBadge } from "./components/shared/runtime-status-badge.js";
import { createSessionStatusCard } from "./components/shared/session-status-card.js";
import { createWelcomePage } from "./pages/welcome-page.js";
import { createVaultPage } from "./pages/vault-page.js";
import { createRuntimePage } from "./pages/runtime-page.js";
import { createSearchPage } from "./pages/search-page.js";
import { createAttachmentPage } from "./pages/attachment-page.js";
import { createChemistryPage } from "./pages/chemistry-page.js";
import { createDiagnosticsPage } from "./pages/diagnostics-page.js";

const POLLING_INTERVAL_MS = 5000;

class AppShell {
  constructor() {
    this._layout = null;
    this._contentArea = null;
    this._runtimeTimer = null;
    this._initialized = false;
    this._sessionTransitioning = false;
    this._lastSessionOpen = undefined;
    this._visibilityHandler = null;
  }

  async init() {
    if (this._initialized) return;
    this._initialized = true;

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

    // 3. Determine initial page based on session state
    const sessionState = store.getSessionState();
    const initialPage = sessionState === "open" ? "vault" : "welcome";
    store.setCurrentPage(initialPage);

    // 4. Build layout
    this._buildLayout();

    // 5. Subscribe to store changes
    store.subscribe(() => this._onStoreUpdate());

    // 6. Start runtime polling if session is open
    if (sessionState === "open") {
      this._startRuntimePolling();
    }

    // 7. Render initial content
    this._renderContent();

    // 8. Visibility change handling
    this._visibilityHandler = () => {
      if (document.visibilityState === "visible") {
        if (store.getSessionState() === "open") {
          runtime.getSummary("app-visibility-resume").then((env) => store.setEnvelope("runtime", env));
        }
      }
    };
    document.addEventListener("visibilitychange", this._visibilityHandler);

    // 9. Expose smoke helpers
    this._exposeSmokeHelpers();
  }

  _buildLayout() {
    const snapshot = store.snapshot();
    const sessionState = store.getSessionState();
    const sessionOpen = sessionState === "open";

    const { element, contentArea } = createAppLayout({
      title: "Chemistry Obsidian",
      currentPage: snapshot.currentPage,
      sessionOpen,
      activeVaultPath: store.getActiveVaultPath(),
      onNavigate: (pageId) => this._navigateTo(pageId),
      onCloseVault: () => this._closeVault()
    });

    this._layout = element;
    this._contentArea = contentArea;

    const root = document.getElementById("app-root");
    if (root) {
      root.innerHTML = "";
      root.appendChild(element);
    }

    this._updateTopBarStatus();
  }

  _onStoreUpdate() {
    const sessionState = store.getSessionState();
    const sessionOpen = sessionState === "open";
    const snapshot = store.snapshot();

    // Update top-bar runtime badge
    this._updateTopBarStatus();

    // If session transitioned, handle page routing and polling
    const prevSessionOpen = this._lastSessionOpen;
    this._lastSessionOpen = sessionOpen;

    if (prevSessionOpen !== sessionOpen) {
      if (sessionOpen) {
        this._startRuntimePolling();
        if (snapshot.currentPage === "welcome") {
          store.setCurrentPage("vault");
        }
      } else {
        this._stopRuntimePolling();
        store.setCurrentPage("welcome");
      }
    }

    this._renderContent();
  }

  _updateTopBarStatus() {
    if (!this._layout) return;
    let badgeContainer = this._layout.querySelector("#runtime-status-badge-container");
    if (!badgeContainer) return;

    badgeContainer.innerHTML = "";
    const runtimeEnv = store.getRuntimeSummary();
    const badge = createRuntimeStatusBadge(runtimeEnv);
    badgeContainer.appendChild(badge);
  }

  _renderContent() {
    if (!this._contentArea) return;
    this._contentArea.innerHTML = "";

    const snapshot = store.snapshot();

    // Host unavailable takes precedence
    if (!snapshot.hostAvailable) {
      const surface = createStateSurface("unavailable", {
        message: "Host unavailable. Preload bridge may be missing.",
        onRetry: () => window.location.reload()
      });
      this._contentArea.appendChild(surface);
      return;
    }

    const currentPage = snapshot.currentPage;
    const sessionState = store.getSessionState();

    // Degraded banner (non-blocking)
    const degradedBanner = this._buildDegradedBanner();
    if (degradedBanner) {
      this._contentArea.appendChild(degradedBanner);
    }

    if (currentPage === "welcome" || sessionState !== "open") {
      const effectiveState = this._sessionTransitioning ? "opening" : sessionState;
      const welcome = createWelcomePage({
        onOpenVault: (path) => this._openVault(path),
        lastError: store.getLastSessionError(),
        sessionState: effectiveState
      });
      this._contentArea.appendChild(welcome);
      return;
    }

    // Session is open and we're on a vault sub-page
    if (currentPage === "runtime") {
      this._contentArea.appendChild(createRuntimePage(store.getRuntimeSummary()));
      return;
    }

    if (currentPage === "search") {
      this._contentArea.appendChild(createSearchPage());
      return;
    }

    if (currentPage === "attachments") {
      this._contentArea.appendChild(createAttachmentPage());
      return;
    }

    if (currentPage === "chemistry") {
      this._contentArea.appendChild(createChemistryPage());
      return;
    }

    if (currentPage === "diagnostics") {
      this._contentArea.appendChild(createDiagnosticsPage());
      return;
    }

    // Default vault page with optional side panel
    const wrapper = document.createElement("div");
    wrapper.style.cssText = "display: grid; grid-template-columns: 1fr 320px; gap: 16px; height: 100%;";

    const mainPane = document.createElement("div");
    mainPane.style.cssText = "min-width: 0; overflow: auto;";
    mainPane.appendChild(createVaultPage({ currentPage }));
    wrapper.appendChild(mainPane);

    const sidePane = document.createElement("div");
    sidePane.style.cssText = "overflow: auto;";
    sidePane.appendChild(createSessionStatusCard(store.getEnvelope("session")));
    wrapper.appendChild(sidePane);

    this._contentArea.appendChild(wrapper);
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

  _navigateTo(pageId) {
    store.setCurrentPage(pageId);
  }

  async _openVault(vaultPath) {
    this._sessionTransitioning = true;
    this._renderContent();

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
    this._renderContent();

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
      navigateTo: (pageName) => this._navigateTo(pageName),
      getHostStoreSnapshot: () => store.snapshot()
    };
  }
}

export const appShell = new AppShell();
