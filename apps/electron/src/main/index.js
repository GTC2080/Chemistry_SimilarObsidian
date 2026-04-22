const fs = require("node:fs");
const path = require("node:path");
const { app, BrowserWindow } = require("electron");
const { HostApi } = require("./host-api");
const { registerHostIpc } = require("./register-host-ipc");
const { registerDesktopIpc } = require("./register-desktop-ipc");
const { HostRuntime } = require("./host-runtime");
const { SECURITY_BASELINE } = require("../shared/host-contract");

const isSmokeRun = process.env.CHEM_OBSIDIAN_HOST_SMOKE === "1";
const rendererDevUrl = typeof process.env.CHEM_OBSIDIAN_RENDERER_URL === "string" &&
  process.env.CHEM_OBSIDIAN_RENDERER_URL.trim()
  ? process.env.CHEM_OBSIDIAN_RENDERER_URL.trim()
  : "";
const startupVaultPath = typeof process.env.CHEM_OBSIDIAN_STARTUP_VAULT === "string" &&
  process.env.CHEM_OBSIDIAN_STARTUP_VAULT.trim()
  ? process.env.CHEM_OBSIDIAN_STARTUP_VAULT.trim()
  : "";
const smokeRunId = typeof process.env.CHEM_OBSIDIAN_HOST_SMOKE_RUN_ID === "string" &&
  process.env.CHEM_OBSIDIAN_HOST_SMOKE_RUN_ID.trim()
  ? process.env.CHEM_OBSIDIAN_HOST_SMOKE_RUN_ID.trim().replace(/[^a-zA-Z0-9_-]/g, "_")
  : "default";
const smokeRendererPath = path.join(__dirname, "../smoke-renderer/index.html");
const builtRendererPath = path.join(__dirname, "../../build/renderer-react/index.html");

function appendHostDebug(label, payload = null) {
  try {
    const debugPath = path.join(app.getPath("userData"), "host-debug.log");
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

function logSmokeStage(stage, details = null) {
  if (!isSmokeRun) {
    return;
  }

  console.log(`ELECTRON_SMOKE_STAGE ${JSON.stringify({
    runId: smokeRunId,
    stage,
    ...(details ?? {})
  })}`);
}

let hostApi;
let hostRuntime;

try {
  hostApi = new HostApi({
    getActiveVaultPath: () => hostRuntime ? hostRuntime.getActiveVaultPath() : null
  });
  hostRuntime = new HostRuntime({
    kernelAdapter: hostApi.getKernelAdapter()
  });
} catch (error) {
  throw error;
}

function getSmokePaths() {
  const smokeRoot = path.join(app.getPath("userData"), "smoke");

  return {
    root: smokeRoot,
    vaultRoot: path.join(smokeRoot, "vault"),
    supportBundlePath: path.join(smokeRoot, "support-bundle.json"),
    resultPath: path.join(smokeRoot, `result-${smokeRunId}.json`),
    progressPath: path.join(smokeRoot, `progress-${smokeRunId}.log`)
  };
}

function makeSmokePdfBytes(pageText) {
  return "%PDF-1.7\n1 0 obj\n<< /Type /Catalog /Title (Smoke PDF) >>\nendobj\n" +
    "2 0 obj\n<< /Type /Page >>\nBT\n/F1 12 Tf\n(" + pageText + ") Tj\nET\nendobj\n%%EOF\n";
}

function makeSmokeJcampBytes(title) {
  return "##JCAMP-DX=5.01\n" +
    "##TITLE=" + title + "\n" +
    "##DATA TYPE=NMR SPECTRUM\n" +
    "##XUNITS=PPM\n" +
    "##YUNITS=INTENSITY\n" +
    "##NPOINTS=4\n";
}

function ensureSmokeVault(smokePaths) {
  const vaultRoot = smokePaths.vaultRoot;
  const notesDir = path.join(vaultRoot, "notes");
  const assetsDir = path.join(vaultRoot, "assets");

  fs.mkdirSync(notesDir, { recursive: true });
  fs.mkdirSync(assetsDir, { recursive: true });
  fs.writeFileSync(
    path.join(assetsDir, "sample.pdf"),
    makeSmokePdfBytes("Smoke PDF Anchor Text"),
    "utf8"
  );
  fs.writeFileSync(
    path.join(assetsDir, "sample.jdx"),
    makeSmokeJcampBytes("Smoke Spectrum"),
    "utf8"
  );
  fs.writeFileSync(
    path.join(notesDir, "example.md"),
    "# Baseline Smoke Note\n\nThis vault exists only for the Electron host smoke path.\n\n" +
      "[PDF](assets/sample.pdf)\n\n" +
      "[Spectrum](assets/sample.jdx)\n",
    "utf8"
  );

  return vaultRoot;
}

function writeSmokeResult(smokePaths, result) {
  fs.mkdirSync(smokePaths.root, { recursive: true });
  fs.writeFileSync(smokePaths.resultPath, JSON.stringify(result, null, 2), "utf8");
}

function appendSmokeProgress(smokePaths, stage, details = null) {
  fs.mkdirSync(smokePaths.root, { recursive: true });
  fs.appendFileSync(smokePaths.progressPath, `${JSON.stringify({
    at_ms: Date.now(),
    stage,
    ...(details ?? {})
  })}\n`, "utf8");
}

if (isSmokeRun) {
  app.disableHardwareAcceleration();
}

registerHostIpc(hostRuntime, hostApi);
registerDesktopIpc();

async function createMainWindow() {
  appendHostDebug("create_main_window.begin", {
    isSmokeRun,
    rendererDevUrl,
    builtRendererPath
  });
  const mainWindow = new BrowserWindow({
    width: 960,
    height: 640,
    show: !isSmokeRun,
    autoHideMenuBar: true,
    frame: false,
    backgroundColor: "#1e1e1e",
    title: "Chemistry_Obsidian Host Shell",
    webPreferences: {
      preload: path.join(__dirname, "../preload/index.js"),
      contextIsolation: SECURITY_BASELINE.contextIsolation,
      nodeIntegration: SECURITY_BASELINE.nodeIntegration,
      sandbox: SECURITY_BASELINE.sandbox,
      enableRemoteModule: SECURITY_BASELINE.enableRemoteModule
    }
  });

  if (typeof mainWindow.removeMenu === "function") {
    mainWindow.removeMenu();
  }

  hostRuntime.bindMainWindow(mainWindow);
  hostRuntime.noteWindowEvent("window_created");

  mainWindow.on("closed", () => {
    hostRuntime.noteWindowEvent("window_closed");
  });

  mainWindow.webContents.on("render-process-gone", (_event, details) => {
    hostRuntime.noteWindowEvent("render_process_gone", {
      reason: details.reason,
      exitCode: details.exitCode
    });
    appendHostDebug("render_process_gone", {
      reason: details.reason,
      exitCode: details.exitCode
    });
  });

  mainWindow.webContents.on("console-message", (_event, level, message, line, sourceId) => {
    // eslint-disable-next-line no-console
    console.log(`RENDERER_CONSOLE ${JSON.stringify({ level, message, line, sourceId })}`);
    appendHostDebug("renderer_console", {
      level,
      message,
      line,
      sourceId
    });
  });

  mainWindow.webContents.on("preload-error", (_event, preloadPath, error) => {
    // eslint-disable-next-line no-console
    console.error(`PRELOAD_ERROR ${JSON.stringify({
      preloadPath,
      error: error && error.stack ? error.stack : String(error)
    })}`);
    appendHostDebug("preload_error", {
      preloadPath,
      error: error && error.stack ? error.stack : String(error)
    });
  });

  mainWindow.webContents.on("did-fail-load", (_event, errorCode, errorDescription, validatedURL, isMainFrame) => {
    // eslint-disable-next-line no-console
    console.error(`DID_FAIL_LOAD ${JSON.stringify({
      errorCode,
      errorDescription,
      validatedURL,
      isMainFrame
    })}`);
    appendHostDebug("did_fail_load", {
      errorCode,
      errorDescription,
      validatedURL,
      isMainFrame
    });
  });

  mainWindow.webContents.on("did-finish-load", () => {
    // eslint-disable-next-line no-console
    console.log(`DID_FINISH_LOAD ${JSON.stringify({
      url: mainWindow.webContents.getURL(),
      isSmokeRun
    })}`);
    appendHostDebug("did_finish_load", {
      url: mainWindow.webContents.getURL(),
      isSmokeRun
    });
  });

  if (isSmokeRun) {
    const smokePaths = getSmokePaths();
    const smokeVaultPath = ensureSmokeVault(smokePaths);
    logSmokeStage("window_created", {
      smokeVaultPath
    });
    appendSmokeProgress(smokePaths, "smoke_window_created", {
      smoke_vault_path: smokeVaultPath,
      run_id: smokeRunId
    });

    mainWindow.webContents.once("did-finish-load", async () => {
      logSmokeStage("did_finish_load");
      appendSmokeProgress(smokePaths, "did_finish_load");

      try {
        logSmokeStage("execute_javascript_begin");
        const smokePayload = await mainWindow.webContents.executeJavaScript(`
          (async () => {
            const wait = (ms) => new Promise((resolve) => setTimeout(resolve, ms));
            async function waitForHostShell() {
              for (let attempt = 0; attempt < 50; attempt += 1) {
                if (window.hostShell) {
                  return window.hostShell;
                }
                await wait(100);
              }

              throw new Error("window.hostShell was not exposed before smoke execution.");
            }

            async function waitForIndexReady() {
              const hostShell = await waitForHostShell();
              for (let attempt = 0; attempt < 50; attempt += 1) {
                const runtimeSummary = await hostShell.runtime.getSummary("smoke-runtime-poll");
                if (
                  runtimeSummary.ok &&
                  runtimeSummary.data?.kernel_runtime?.session_state === "open" &&
                  runtimeSummary.data?.kernel_runtime?.index_state === "ready"
                ) {
                  return runtimeSummary;
                }
                await wait(100);
              }

              return hostShell.runtime.getSummary("smoke-runtime-poll-final");
            }

            const hostShell = await waitForHostShell();
            const [bootstrap, runtime, session] = await Promise.all([
              hostShell.bootstrap.getInfo(),
              hostShell.runtime.getSummary(),
              hostShell.session.getStatus()
            ]);
            const [filesList, filesRead, filesRecent, search, attachments, attachmentGet, attachmentRefs, attachmentReferrers] = await Promise.all([
              hostShell.files.listEntries({ limit: 5 }, "smoke-files-list"),
              hostShell.files.readNote({ relPath: "notes/example.md" }, "smoke-files-read"),
              hostShell.files.listRecent({ limit: 5 }, "smoke-files-recent"),
              hostShell.search.query({ query: "baseline", limit: 5 }, "smoke-search"),
              hostShell.attachments.list({ limit: 5 }, "smoke-attachments-list"),
              hostShell.attachments.get({ attachmentRelPath: "assets/sample.pdf" }, "smoke-attachments-get"),
              hostShell.attachments.queryNoteRefs({ noteRelPath: "notes/example.md", limit: 5 }, "smoke-attachments-note-refs"),
              hostShell.attachments.queryReferrers({ attachmentRelPath: "assets/sample.pdf", limit: 5 }, "smoke-attachments-referrers")
            ]);
            const [pdfMetadata, pdfNoteRefs, pdfReferrers] = await Promise.all([
              hostShell.pdf.getMetadata({ attachmentRelPath: "assets/sample.pdf" }, "smoke-pdf-metadata"),
              hostShell.pdf.queryNoteSourceRefs({ noteRelPath: "notes/example.md", limit: 5 }, "smoke-pdf-note-refs"),
              hostShell.pdf.queryReferrers({ attachmentRelPath: "assets/sample.pdf", limit: 5 }, "smoke-pdf-referrers")
            ]);
            const [chemMetadata, chemSpectra, chemSpectrum, chemNoteRefs, chemReferrers] = await Promise.all([
              hostShell.chemistry.queryMetadata({ attachmentRelPath: "assets/sample.jdx", limit: 5 }, "smoke-chem-metadata"),
              hostShell.chemistry.listSpectra({ limit: 5 }, "smoke-chem-list"),
              hostShell.chemistry.getSpectrum({ attachmentRelPath: "assets/sample.jdx" }, "smoke-chem-get"),
              hostShell.chemistry.queryNoteRefs({ noteRelPath: "notes/example.md", limit: 5 }, "smoke-chem-note-refs"),
              hostShell.chemistry.queryReferrers({ attachmentRelPath: "assets/sample.jdx", limit: 5 }, "smoke-chem-referrers")
            ]);
            const [diagnosticsExport, rebuildStatus, rebuildStart, rebuildWait] = await Promise.all([
              hostShell.diagnostics.exportSupportBundle(
                { outputPath: ${JSON.stringify(smokePaths.supportBundlePath)} },
                "smoke-diagnostics-export"
              ),
              hostShell.rebuild.getStatus("smoke-rebuild-status"),
              hostShell.rebuild.start("smoke-rebuild-start"),
              hostShell.rebuild.wait({ timeoutMs: 1_000 }, "smoke-rebuild-wait")
            ]);
            const openAttempt = await hostShell.session.openVault(
              ${JSON.stringify(smokeVaultPath)},
              "smoke-open"
            );
            const runtimeAfterOpen = await waitForIndexReady();
            const rebuildStatusAfterOpen = await hostShell.rebuild.getStatus("smoke-rebuild-status-after-open");
            const postOpenSession = await hostShell.session.getStatus("smoke-post-open");
            const [filesListAfterOpen, filesReadAfterOpen, filesRecentAfterOpen, searchAfterOpen, attachmentsAfterOpen, attachmentGetAfterOpen, attachmentRefsAfterOpen, attachmentReferrersAfterOpen] = await Promise.all([
              hostShell.files.listEntries({ limit: 5 }, "smoke-files-list-after-open"),
              hostShell.files.readNote({ relPath: "notes/example.md" }, "smoke-files-read-after-open"),
              hostShell.files.listRecent({ limit: 5 }, "smoke-files-recent-after-open"),
              hostShell.search.query({ query: "baseline", limit: 5 }, "smoke-search-after-open"),
              hostShell.attachments.list({ limit: 5 }, "smoke-attachments-list-after-open"),
              hostShell.attachments.get({ attachmentRelPath: "assets/sample.pdf" }, "smoke-attachments-get-after-open"),
              hostShell.attachments.queryNoteRefs({ noteRelPath: "notes/example.md", limit: 5 }, "smoke-attachments-note-refs-after-open"),
              hostShell.attachments.queryReferrers({ attachmentRelPath: "assets/sample.pdf", limit: 5 }, "smoke-attachments-referrers-after-open")
            ]);
            const [pdfMetadataAfterOpen, pdfNoteRefsAfterOpen, pdfReferrersAfterOpen] = await Promise.all([
              hostShell.pdf.getMetadata({ attachmentRelPath: "assets/sample.pdf" }, "smoke-pdf-metadata-after-open"),
              hostShell.pdf.queryNoteSourceRefs({ noteRelPath: "notes/example.md", limit: 5 }, "smoke-pdf-note-refs-after-open"),
              hostShell.pdf.queryReferrers({ attachmentRelPath: "assets/sample.pdf" }, "smoke-pdf-referrers-after-open")
            ]);
            const [chemMetadataAfterOpen, chemSpectraAfterOpen, chemSpectrumAfterOpen, chemNoteRefsAfterOpen, chemReferrersAfterOpen] = await Promise.all([
              hostShell.chemistry.queryMetadata({ attachmentRelPath: "assets/sample.jdx", limit: 5 }, "smoke-chem-metadata-after-open"),
              hostShell.chemistry.listSpectra({ limit: 5 }, "smoke-chem-list-after-open"),
              hostShell.chemistry.getSpectrum({ attachmentRelPath: "assets/sample.jdx" }, "smoke-chem-get-after-open"),
              hostShell.chemistry.queryNoteRefs({ noteRelPath: "notes/example.md", limit: 5 }, "smoke-chem-note-refs-after-open"),
              hostShell.chemistry.queryReferrers({ attachmentRelPath: "assets/sample.jdx", limit: 5 }, "smoke-chem-referrers-after-open")
            ]);
            const diagnosticsExportAfterOpen = await hostShell.diagnostics.exportSupportBundle(
              { outputPath: ${JSON.stringify(smokePaths.supportBundlePath)} },
              "smoke-diagnostics-export-after-open"
            );
            const rebuildStartAfterOpen = await hostShell.rebuild.start("smoke-rebuild-start-after-open");
            const rebuildWaitAfterOpen = await hostShell.rebuild.wait(
              { timeoutMs: 10_000 },
              "smoke-rebuild-wait-after-open"
            );
            const rebuildStatusAfterWait = await hostShell.rebuild.getStatus("smoke-rebuild-status-after-wait");
            const closeAttempt = await hostShell.session.closeVault("smoke-close");

            return {
              title: document.title,
              marker: document.getElementById("host-shell-marker")?.textContent ?? null,
              detail: document.getElementById("host-shell-detail")?.textContent ?? null,
              bootstrap,
              runtime,
              session,
              filesList,
              filesRead,
              filesRecent,
              search,
              attachments,
              attachmentGet,
              attachmentRefs,
              attachmentReferrers,
              pdfMetadata,
              pdfNoteRefs,
              pdfReferrers,
              chemMetadata,
              chemSpectra,
              chemSpectrum,
              chemNoteRefs,
              chemReferrers,
              diagnosticsExport,
              rebuildStatus,
              rebuildStart,
              rebuildWait,
              openAttempt,
              runtimeAfterOpen,
              rebuildStatusAfterOpen,
              postOpenSession,
              filesListAfterOpen,
              filesReadAfterOpen,
              filesRecentAfterOpen,
              searchAfterOpen,
              attachmentsAfterOpen,
              attachmentGetAfterOpen,
              attachmentRefsAfterOpen,
              attachmentReferrersAfterOpen,
              pdfMetadataAfterOpen,
              pdfNoteRefsAfterOpen,
              pdfReferrersAfterOpen,
              chemMetadataAfterOpen,
              chemSpectraAfterOpen,
              chemSpectrumAfterOpen,
              chemNoteRefsAfterOpen,
              chemReferrersAfterOpen,
              diagnosticsExportAfterOpen,
              rebuildStartAfterOpen,
              rebuildWaitAfterOpen,
              rebuildStatusAfterWait,
              closeAttempt
            };
          })()
        `);

        logSmokeStage("execute_javascript_success");
        appendSmokeProgress(smokePaths, "execute_javascript_success");
        writeSmokeResult(smokePaths, {
          ok: true,
          runId: smokeRunId,
          payload: smokePayload
        });
        console.log(`ELECTRON_SMOKE_OK ${JSON.stringify(smokePayload)}`);
      } catch (error) {
        logSmokeStage("execute_javascript_failure", {
          error: error && error.message ? error.message : String(error)
        });
        appendSmokeProgress(smokePaths, "execute_javascript_failure", {
          error: error && error.stack ? error.stack : String(error)
        });
        writeSmokeResult(smokePaths, {
          ok: false,
          runId: smokeRunId,
          error: error && error.stack ? error.stack : String(error)
        });
        console.error(`ELECTRON_SMOKE_FAIL ${error && error.stack ? error.stack : error}`);
        process.exitCode = 1;
      } finally {
        appendSmokeProgress(smokePaths, "smoke_window_close_requested");
        setTimeout(() => {
          if (!mainWindow.isDestroyed()) {
            mainWindow.close();
          }
        }, 200);
      }
    });
  }

  if (startupVaultPath && !isSmokeRun) {
    const startupOpen = await hostRuntime.openVault({
      vaultPath: startupVaultPath,
      request_id: "startup-open-vault"
    });

    if (!startupOpen.ok) {
      // eslint-disable-next-line no-console
      console.error(`STARTUP_VAULT_OPEN_FAIL ${JSON.stringify(startupOpen.error)}`);
    }
  }

  if (isSmokeRun) {
    await mainWindow.loadFile(smokeRendererPath);
    appendHostDebug("create_main_window.loaded_smoke");
    return;
  }

  if (rendererDevUrl) {
    await mainWindow.loadURL(rendererDevUrl);
    appendHostDebug("create_main_window.loaded_dev_url", {
      url: rendererDevUrl
    });
    return;
  }

  if (!fs.existsSync(builtRendererPath)) {
    throw new Error(`Renderer build missing at ${builtRendererPath}. Run npm run renderer:build or npm start.`);
  }

  await mainWindow.loadFile(builtRendererPath);
  appendHostDebug("create_main_window.loaded_built_renderer");
}

app.whenReady().then(async () => {
  appendHostDebug("app_when_ready.begin");
  logSmokeStage("app_when_ready");
  hostRuntime.markReady();
  await createMainWindow();
  appendHostDebug("app_when_ready.done");

  app.on("activate", () => {
    if (BrowserWindow.getAllWindows().length === 0) {
      void createMainWindow();
    }
  });
});

app.on("before-quit", async () => {
  appendHostDebug("before_quit");
  await hostRuntime.prepareForShutdown();
});

app.on("window-all-closed", () => {
  appendHostDebug("window_all_closed");
  if (process.platform !== "darwin") {
    app.quit();
  }
});
