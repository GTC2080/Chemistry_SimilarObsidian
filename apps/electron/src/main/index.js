const fs = require("node:fs");
const path = require("node:path");
const { app, BrowserWindow } = require("electron");
const { HostApi } = require("./host-api");
const { registerHostIpc } = require("./register-host-ipc");
const { HostRuntime } = require("./host-runtime");
const { SECURITY_BASELINE } = require("../shared/host-contract");

const isSmokeRun = process.env.CHEM_OBSIDIAN_HOST_SMOKE === "1";
const startupVaultPath = typeof process.env.CHEM_OBSIDIAN_STARTUP_VAULT === "string" &&
  process.env.CHEM_OBSIDIAN_STARTUP_VAULT.trim()
  ? process.env.CHEM_OBSIDIAN_STARTUP_VAULT.trim()
  : "";
const smokeRunId = typeof process.env.CHEM_OBSIDIAN_HOST_SMOKE_RUN_ID === "string" &&
  process.env.CHEM_OBSIDIAN_HOST_SMOKE_RUN_ID.trim()
  ? process.env.CHEM_OBSIDIAN_HOST_SMOKE_RUN_ID.trim().replace(/[^a-zA-Z0-9_-]/g, "_")
  : "default";

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

async function createMainWindow() {
  const mainWindow = new BrowserWindow({
    width: 960,
    height: 640,
    show: !isSmokeRun,
    autoHideMenuBar: true,
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
            async function waitForIndexReady() {
              for (let attempt = 0; attempt < 50; attempt += 1) {
                const runtimeSummary = await window.hostShell.runtime.getSummary("smoke-runtime-poll");
                if (
                  runtimeSummary.ok &&
                  runtimeSummary.data?.kernel_runtime?.session_state === "open" &&
                  runtimeSummary.data?.kernel_runtime?.index_state === "ready"
                ) {
                  return runtimeSummary;
                }
                await wait(100);
              }

              return window.hostShell.runtime.getSummary("smoke-runtime-poll-final");
            }

            const [bootstrap, runtime, session] = await Promise.all([
              window.hostShell.bootstrap.getInfo(),
              window.hostShell.runtime.getSummary(),
              window.hostShell.session.getStatus()
            ]);
            const [filesList, filesRead, filesRecent, search, attachments, attachmentGet, attachmentRefs, attachmentReferrers] = await Promise.all([
              window.hostShell.files.listEntries({ limit: 5 }, "smoke-files-list"),
              window.hostShell.files.readNote({ relPath: "notes/example.md" }, "smoke-files-read"),
              window.hostShell.files.listRecent({ limit: 5 }, "smoke-files-recent"),
              window.hostShell.search.query({ query: "baseline", limit: 5 }, "smoke-search"),
              window.hostShell.attachments.list({ limit: 5 }, "smoke-attachments-list"),
              window.hostShell.attachments.get({ attachmentRelPath: "assets/sample.pdf" }, "smoke-attachments-get"),
              window.hostShell.attachments.queryNoteRefs({ noteRelPath: "notes/example.md", limit: 5 }, "smoke-attachments-note-refs"),
              window.hostShell.attachments.queryReferrers({ attachmentRelPath: "assets/sample.pdf", limit: 5 }, "smoke-attachments-referrers")
            ]);
            const [pdfMetadata, pdfNoteRefs, pdfReferrers] = await Promise.all([
              window.hostShell.pdf.getMetadata({ attachmentRelPath: "assets/sample.pdf" }, "smoke-pdf-metadata"),
              window.hostShell.pdf.queryNoteSourceRefs({ noteRelPath: "notes/example.md", limit: 5 }, "smoke-pdf-note-refs"),
              window.hostShell.pdf.queryReferrers({ attachmentRelPath: "assets/sample.pdf", limit: 5 }, "smoke-pdf-referrers")
            ]);
            const [chemMetadata, chemSpectra, chemSpectrum, chemNoteRefs, chemReferrers] = await Promise.all([
              window.hostShell.chemistry.queryMetadata({ attachmentRelPath: "assets/sample.jdx", limit: 5 }, "smoke-chem-metadata"),
              window.hostShell.chemistry.listSpectra({ limit: 5 }, "smoke-chem-list"),
              window.hostShell.chemistry.getSpectrum({ attachmentRelPath: "assets/sample.jdx" }, "smoke-chem-get"),
              window.hostShell.chemistry.queryNoteRefs({ noteRelPath: "notes/example.md", limit: 5 }, "smoke-chem-note-refs"),
              window.hostShell.chemistry.queryReferrers({ attachmentRelPath: "assets/sample.jdx", limit: 5 }, "smoke-chem-referrers")
            ]);
            const [diagnosticsExport, rebuildStatus, rebuildStart, rebuildWait] = await Promise.all([
              window.hostShell.diagnostics.exportSupportBundle(
                { outputPath: ${JSON.stringify(smokePaths.supportBundlePath)} },
                "smoke-diagnostics-export"
              ),
              window.hostShell.rebuild.getStatus("smoke-rebuild-status"),
              window.hostShell.rebuild.start("smoke-rebuild-start"),
              window.hostShell.rebuild.wait({ timeoutMs: 1_000 }, "smoke-rebuild-wait")
            ]);
            const openAttempt = await window.hostShell.session.openVault(
              ${JSON.stringify(smokeVaultPath)},
              "smoke-open"
            );
            const runtimeAfterOpen = await waitForIndexReady();
            const rebuildStatusAfterOpen = await window.hostShell.rebuild.getStatus("smoke-rebuild-status-after-open");
            const postOpenSession = await window.hostShell.session.getStatus("smoke-post-open");
            const [filesListAfterOpen, filesReadAfterOpen, filesRecentAfterOpen, searchAfterOpen, attachmentsAfterOpen, attachmentGetAfterOpen, attachmentRefsAfterOpen, attachmentReferrersAfterOpen] = await Promise.all([
              window.hostShell.files.listEntries({ limit: 5 }, "smoke-files-list-after-open"),
              window.hostShell.files.readNote({ relPath: "notes/example.md" }, "smoke-files-read-after-open"),
              window.hostShell.files.listRecent({ limit: 5 }, "smoke-files-recent-after-open"),
              window.hostShell.search.query({ query: "baseline", limit: 5 }, "smoke-search-after-open"),
              window.hostShell.attachments.list({ limit: 5 }, "smoke-attachments-list-after-open"),
              window.hostShell.attachments.get({ attachmentRelPath: "assets/sample.pdf" }, "smoke-attachments-get-after-open"),
              window.hostShell.attachments.queryNoteRefs({ noteRelPath: "notes/example.md", limit: 5 }, "smoke-attachments-note-refs-after-open"),
              window.hostShell.attachments.queryReferrers({ attachmentRelPath: "assets/sample.pdf", limit: 5 }, "smoke-attachments-referrers-after-open")
            ]);
            const [pdfMetadataAfterOpen, pdfNoteRefsAfterOpen, pdfReferrersAfterOpen] = await Promise.all([
              window.hostShell.pdf.getMetadata({ attachmentRelPath: "assets/sample.pdf" }, "smoke-pdf-metadata-after-open"),
              window.hostShell.pdf.queryNoteSourceRefs({ noteRelPath: "notes/example.md", limit: 5 }, "smoke-pdf-note-refs-after-open"),
              window.hostShell.pdf.queryReferrers({ attachmentRelPath: "assets/sample.pdf", limit: 5 }, "smoke-pdf-referrers-after-open")
            ]);
            const [chemMetadataAfterOpen, chemSpectraAfterOpen, chemSpectrumAfterOpen, chemNoteRefsAfterOpen, chemReferrersAfterOpen] = await Promise.all([
              window.hostShell.chemistry.queryMetadata({ attachmentRelPath: "assets/sample.jdx", limit: 5 }, "smoke-chem-metadata-after-open"),
              window.hostShell.chemistry.listSpectra({ limit: 5 }, "smoke-chem-list-after-open"),
              window.hostShell.chemistry.getSpectrum({ attachmentRelPath: "assets/sample.jdx" }, "smoke-chem-get-after-open"),
              window.hostShell.chemistry.queryNoteRefs({ noteRelPath: "notes/example.md", limit: 5 }, "smoke-chem-note-refs-after-open"),
              window.hostShell.chemistry.queryReferrers({ attachmentRelPath: "assets/sample.jdx", limit: 5 }, "smoke-chem-referrers-after-open")
            ]);
            const diagnosticsExportAfterOpen = await window.hostShell.diagnostics.exportSupportBundle(
              { outputPath: ${JSON.stringify(smokePaths.supportBundlePath)} },
              "smoke-diagnostics-export-after-open"
            );
            const rebuildStartAfterOpen = await window.hostShell.rebuild.start("smoke-rebuild-start-after-open");
            const rebuildWaitAfterOpen = await window.hostShell.rebuild.wait(
              { timeoutMs: 10_000 },
              "smoke-rebuild-wait-after-open"
            );
            const rebuildStatusAfterWait = await window.hostShell.rebuild.getStatus("smoke-rebuild-status-after-wait");
            const closeAttempt = await window.hostShell.session.closeVault("smoke-close");

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

  await mainWindow.loadFile(path.join(__dirname, "../renderer/index.html"));
}

app.whenReady().then(async () => {
  logSmokeStage("app_when_ready");
  hostRuntime.markReady();
  await createMainWindow();

  app.on("activate", () => {
    if (BrowserWindow.getAllWindows().length === 0) {
      void createMainWindow();
    }
  });
});

app.on("before-quit", async () => {
  await hostRuntime.prepareForShutdown();
});

app.on("window-all-closed", () => {
  if (process.platform !== "darwin") {
    app.quit();
  }
});
