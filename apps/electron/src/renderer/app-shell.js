/**
 * app-shell.js
 *
 * Root module: routes between Launcher (no vault) and Workspace (vault open).
 */

import { store } from "./state/host-store.js";
import { bootstrap, files, runtime, session } from "./services/host-api-client.js";
import { createStateSurface } from "./components/shared/state-surface.js";
import { createLauncherShell } from "./components/layout/launcher-shell.js";
import { createRecentVaultsList } from "./pages/launcher/recent-vaults-list.js";
import { createLauncherPage } from "./pages/launcher/launcher-page.js";
import { createWorkspaceShell } from "./components/layout/workspace-shell.js";
import { createFilesPage } from "./pages/files/files-page.js";
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
    this._currentFilesContentId = null;
    this._filesSurfaceState = createEmptyFilesSurfaceState();
    this._filesSurfaceRequestToken = 0;
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
      void this._refreshFilesSurface({ preserveSelection: false });
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
        this._resetFilesSurfaceState();
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

    const bootstrapEnv = store.getBootstrapInfo();
    const hostVersion = bootstrapEnv && bootstrapEnv.ok
      ? bootstrapEnv.data?.host_version ?? null
      : null;
    const launcherState = this._sessionTransitioning ? "opening_vault" : "no_vault_open";

    const { element, rail, stage } = createLauncherShell({
      hostVersion,
      launcherState
    });

    const recentList = createRecentVaultsList({
      onOpen: (path) => this._openVault(path),
      disabled: this._sessionTransitioning
    });

    const launcherPage = createLauncherPage({
      onOpenVault: (path) => this._openVault(path),
      lastError: store.getLastSessionError(),
      isOpening: this._sessionTransitioning,
      hostVersion,
      launcherState
    });

    rail.appendChild(recentList);
    stage.appendChild(launcherPage);
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
      filesSurfaceState: this._filesSurfaceState,
      currentFilesContentId: this._currentFilesContentId,
      onSelectFilesContent: (contentId) => {
        void this._selectFilesContent(contentId);
      },
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
      container.appendChild(createFilesPage({
        vaultPath: store.getActiveVaultPath(),
        runtimeEnvelope: store.getRuntimeSummary(),
        filesSurfaceState: this._filesSurfaceState,
        currentContentId: this._currentFilesContentId,
        onSelectContent: (contentId) => {
          void this._selectFilesContent(contentId);
        }
      }));
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
      padding: 12px 14px;
      margin-bottom: 14px;
      border-radius: 14px;
      background: linear-gradient(180deg, rgba(71, 45, 16, 0.88), rgba(56, 36, 14, 0.88));
      border: 1px solid rgba(250, 204, 21, 0.24);
      color: #fcd34d;
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
      await this._refreshFilesSurface({ preserveSelection: false });
    }

    return result;
  }

  async _closeVault() {
    this._sessionTransitioning = true;
    this._renderMode();

    const result = await session.closeVault("app-close-vault");
    this._sessionTransitioning = false;
    store.setEnvelope("session", result);

    if (result.ok) {
      this._resetFilesSurfaceState();
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

  async _refreshFilesSurface(opts = {}) {
    const { preserveSelection = true } = opts;
    if (store.getSessionState() !== "open") {
      this._resetFilesSurfaceState();
      this._renderMode();
      return;
    }

    const requestToken = ++this._filesSurfaceRequestToken;
    this._filesSurfaceState = {
      ...this._filesSurfaceState,
      loading: true,
      loadingSelection: false,
      currentNoteEnvelope: null,
      selectionError: null
    };
    this._renderMode();

    const [entriesEnvelope, recentEnvelope] = await Promise.all([
      files.listEntries({ limit: 32 }, "renderer-files-root"),
      files.listRecent({ limit: 12 }, "renderer-files-recent")
    ]);

    if (!this._acceptFilesSurfaceResult(requestToken)) {
      return;
    }

    const preferredSelection = preserveSelection ? this._currentFilesContentId : null;
    const selectedRelPath = chooseFilesSelection(preferredSelection, entriesEnvelope, recentEnvelope);
    this._currentFilesContentId = selectedRelPath;

    const selectedEntry = findFilesEntry(selectedRelPath, entriesEnvelope, recentEnvelope);
    const currentNoteEnvelope = await this._loadCurrentNoteEnvelope(selectedRelPath, selectedEntry, requestToken);
    if (!this._acceptFilesSurfaceResult(requestToken)) {
      return;
    }

    this._filesSurfaceState = {
      loading: false,
      loadingSelection: false,
      entriesEnvelope,
      recentEnvelope,
      currentNoteEnvelope,
      selectedRelPath,
      selectedEntry,
      selectionError: null
    };
    this._renderMode();
  }

  async _selectFilesContent(contentId) {
    if (store.getSessionState() !== "open") {
      return;
    }

    const selectedRelPath = normalizeSelectionKey(contentId);
    this._currentFilesContentId = selectedRelPath;
    const requestToken = ++this._filesSurfaceRequestToken;
    const selectedEntry = findFilesEntry(
      selectedRelPath,
      this._filesSurfaceState.entriesEnvelope,
      this._filesSurfaceState.recentEnvelope
    );

    this._filesSurfaceState = {
      ...this._filesSurfaceState,
      loading: false,
      loadingSelection: true,
      selectedRelPath,
      selectedEntry,
      currentNoteEnvelope: null,
      selectionError: null
    };
    this._renderMode();

    const currentNoteEnvelope = await this._loadCurrentNoteEnvelope(selectedRelPath, selectedEntry, requestToken);
    if (!this._acceptFilesSurfaceResult(requestToken)) {
      return;
    }

    this._filesSurfaceState = {
      ...this._filesSurfaceState,
      loadingSelection: false,
      selectedRelPath,
      selectedEntry,
      currentNoteEnvelope,
      selectionError: null
    };
    this._renderMode();
  }

  async _loadCurrentNoteEnvelope(selectedRelPath, selectedEntry, requestToken) {
    if (!selectedRelPath || !isReadableNoteSelection(selectedRelPath, selectedEntry)) {
      return null;
    }

    const currentNoteEnvelope = await files.readNote(
      { relPath: selectedRelPath },
      `renderer-files-read-${requestToken}`
    );
    return this._acceptFilesSurfaceResult(requestToken) ? currentNoteEnvelope : null;
  }

  _acceptFilesSurfaceResult(requestToken) {
    return requestToken === this._filesSurfaceRequestToken && store.getSessionState() === "open";
  }

  _resetFilesSurfaceState() {
    this._filesSurfaceRequestToken += 1;
    this._currentFilesContentId = null;
    this._filesSurfaceState = createEmptyFilesSurfaceState();
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

function createEmptyFilesSurfaceState() {
  return {
    loading: false,
    loadingSelection: false,
    entriesEnvelope: null,
    recentEnvelope: null,
    currentNoteEnvelope: null,
    selectedRelPath: null,
    selectedEntry: null,
    selectionError: null
  };
}

function chooseFilesSelection(previousSelection, entriesEnvelope, recentEnvelope) {
  const normalizedPrevious = normalizeSelectionKey(previousSelection);
  if (normalizedPrevious && findFilesEntry(normalizedPrevious, entriesEnvelope, recentEnvelope)) {
    return normalizedPrevious;
  }

  const firstRecentNote = recentEnvelope?.ok ? recentEnvelope.data?.items?.[0]?.relPath ?? null : null;
  if (typeof firstRecentNote === "string" && firstRecentNote.trim()) {
    return firstRecentNote.trim();
  }

  if (entriesEnvelope?.ok) {
    const firstReadableRootEntry = (entriesEnvelope.data?.items ?? []).find((item) => isReadableNoteSelection(item.relPath, item));
    if (firstReadableRootEntry?.relPath) {
      return firstReadableRootEntry.relPath;
    }
  }

  return null;
}

function findFilesEntry(relPath, entriesEnvelope, recentEnvelope) {
  const normalizedRelPath = normalizeSelectionKey(relPath);
  if (!normalizedRelPath) {
    return null;
  }

  const recentItems = recentEnvelope?.ok ? recentEnvelope.data?.items ?? [] : [];
  const entryItems = entriesEnvelope?.ok ? entriesEnvelope.data?.items ?? [] : [];
  return recentItems.find((item) => item.relPath === normalizedRelPath)
    ?? entryItems.find((item) => item.relPath === normalizedRelPath)
    ?? { relPath: normalizedRelPath, name: baseName(normalizedRelPath), title: baseName(normalizedRelPath), kind: inferEntryKind(normalizedRelPath) };
}

function normalizeSelectionKey(value) {
  return typeof value === "string" && value.trim() ? value.trim() : null;
}

function isReadableNoteSelection(relPath, entry = null) {
  if (entry?.kind === "note") {
    return true;
  }

  return typeof relPath === "string" && relPath.toLowerCase().endsWith(".md");
}

function inferEntryKind(relPath) {
  if (typeof relPath === "string" && relPath.toLowerCase().endsWith(".md")) {
    return "note";
  }

  return "entry";
}

function baseName(filePath) {
  if (!filePath || typeof filePath !== "string") {
    return "";
  }

  const parts = filePath.split(/[\\/]/).filter(Boolean);
  return parts.length > 0 ? parts[parts.length - 1] : filePath;
}
