const fs = require("node:fs");
const path = require("node:path");
const { app, BrowserWindow } = require("electron");
const { HostApi } = require("./host-api");
const { registerHostIpc } = require("./register-host-ipc");
const { HostRuntime } = require("./host-runtime");
const { SECURITY_BASELINE } = require("../shared/host-contract");

const isSmokeRun = process.env.ELECTRON_SMOKE === "1";
const hostApi = new HostApi();
const hostRuntime = new HostRuntime({
  kernelAdapter: hostApi.getKernelAdapter()
});

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

function ensureSmokeVault() {
  const vaultRoot = path.resolve(__dirname, "../../out/smoke-vault");
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

if (isSmokeRun) {
  app.disableHardwareAcceleration();
}

registerHostIpc(hostRuntime, hostApi);

function createMainWindow() {
  const mainWindow = new BrowserWindow({
    width: 960,
    height: 640,
    show: !isSmokeRun,
    title: "Chemistry_Obsidian Host Shell",
    webPreferences: {
      preload: path.join(__dirname, "../preload/index.js"),
      contextIsolation: SECURITY_BASELINE.contextIsolation,
      nodeIntegration: SECURITY_BASELINE.nodeIntegration,
      sandbox: SECURITY_BASELINE.sandbox,
      enableRemoteModule: SECURITY_BASELINE.enableRemoteModule
    }
  });

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

  mainWindow.loadFile(path.join(__dirname, "../renderer/index.html"));

  if (isSmokeRun) {
    const smokeVaultPath = ensureSmokeVault();

    mainWindow.webContents.once("did-finish-load", async () => {
      try {
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
            const [search, attachments, attachmentGet, attachmentRefs, attachmentReferrers] = await Promise.all([
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
                { outputPath: "E:/测试/Chemistry_Obsidian/apps/electron/out/support-bundle.json" },
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
            const [searchAfterOpen, attachmentsAfterOpen, attachmentGetAfterOpen, attachmentRefsAfterOpen, attachmentReferrersAfterOpen] = await Promise.all([
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
              { outputPath: "E:/测试/Chemistry_Obsidian/apps/electron/out/support-bundle.json" },
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

        console.log(`ELECTRON_SMOKE_OK ${JSON.stringify(smokePayload)}`);
      } catch (error) {
        console.error(`ELECTRON_SMOKE_FAIL ${error && error.stack ? error.stack : error}`);
        process.exitCode = 1;
      } finally {
        setTimeout(() => {
          if (!mainWindow.isDestroyed()) {
            mainWindow.close();
          }
        }, 200);
      }
    });
  }
}

app.whenReady().then(() => {
  hostRuntime.markReady();
  createMainWindow();

  app.on("activate", () => {
    if (BrowserWindow.getAllWindows().length === 0) {
      createMainWindow();
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
