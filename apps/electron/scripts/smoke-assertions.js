const fs = require("node:fs");

function fail(message) {
  throw new Error(`Smoke assertion failed: ${message}`);
}

function expect(condition, message) {
  if (!condition) {
    fail(message);
  }
}

function expectSuccessEnvelope(envelope, label) {
  expect(envelope && envelope.ok === true, `${label} should be a success envelope.`);
  expect(envelope.error === null, `${label} should not carry an error payload.`);
  expect(Object.prototype.hasOwnProperty.call(envelope, "data"), `${label} should carry data.`);
}

function expectFailureEnvelope(envelope, label, code) {
  expect(envelope && envelope.ok === false, `${label} should be a failure envelope.`);
  expect(envelope.data === null, `${label} should not carry data when failing.`);
  expect(envelope.error && envelope.error.code === code, `${label} should fail with ${code}.`);
}

function expectArrayIncludes(values, expected, label) {
  expect(Array.isArray(values), `${label} should be an array.`);
  for (const value of expected) {
    expect(values.includes(value), `${label} should include ${value}.`);
  }
}

function getExpectedPreOpenReadFailureCode(payload) {
  const binding = payload.runtime?.data?.kernel_binding ?? {};
  if (binding.attached === true) {
    return "HOST_SESSION_NOT_OPEN";
  }

  return binding.failure_code ?? "HOST_KERNEL_ADAPTER_UNAVAILABLE";
}

function assertSmokePayload(payload) {
  expect(payload && typeof payload === "object", "smoke payload should be an object.");
  expect(payload.title === "Chemistry_Obsidian Host Shell", "window title should match the host shell title.");
  expect(payload.marker === "Electron host shell baseline", "renderer marker should expose the baseline marker.");

  expectSuccessEnvelope(payload.bootstrap, "bootstrap");
  expectSuccessEnvelope(payload.runtime, "runtime");
  expectSuccessEnvelope(payload.session, "session");
  expectSuccessEnvelope(payload.rebuildStatus, "rebuildStatus");
  expectSuccessEnvelope(payload.openAttempt, "openAttempt");
  expectSuccessEnvelope(payload.runtimeAfterOpen, "runtimeAfterOpen");
  expectSuccessEnvelope(payload.rebuildStatusAfterOpen, "rebuildStatusAfterOpen");
  expectSuccessEnvelope(payload.postOpenSession, "postOpenSession");
  expectSuccessEnvelope(payload.searchAfterOpen, "searchAfterOpen");
  expectSuccessEnvelope(payload.attachmentsAfterOpen, "attachmentsAfterOpen");
  expectSuccessEnvelope(payload.attachmentGetAfterOpen, "attachmentGetAfterOpen");
  expectSuccessEnvelope(payload.attachmentRefsAfterOpen, "attachmentRefsAfterOpen");
  expectSuccessEnvelope(payload.attachmentReferrersAfterOpen, "attachmentReferrersAfterOpen");
  expectSuccessEnvelope(payload.pdfMetadataAfterOpen, "pdfMetadataAfterOpen");
  expectSuccessEnvelope(payload.pdfNoteRefsAfterOpen, "pdfNoteRefsAfterOpen");
  expectSuccessEnvelope(payload.pdfReferrersAfterOpen, "pdfReferrersAfterOpen");
  expectSuccessEnvelope(payload.chemMetadataAfterOpen, "chemMetadataAfterOpen");
  expectSuccessEnvelope(payload.chemSpectraAfterOpen, "chemSpectraAfterOpen");
  expectSuccessEnvelope(payload.chemSpectrumAfterOpen, "chemSpectrumAfterOpen");
  expectSuccessEnvelope(payload.chemNoteRefsAfterOpen, "chemNoteRefsAfterOpen");
  expectSuccessEnvelope(payload.chemReferrersAfterOpen, "chemReferrersAfterOpen");
  expectSuccessEnvelope(payload.diagnosticsExportAfterOpen, "diagnosticsExportAfterOpen");
  expectSuccessEnvelope(payload.rebuildStartAfterOpen, "rebuildStartAfterOpen");
  expectSuccessEnvelope(payload.rebuildWaitAfterOpen, "rebuildWaitAfterOpen");
  expectSuccessEnvelope(payload.rebuildStatusAfterWait, "rebuildStatusAfterWait");
  expectSuccessEnvelope(payload.closeAttempt, "closeAttempt");

  const expectedPreOpenReadFailureCode = getExpectedPreOpenReadFailureCode(payload);

  expectFailureEnvelope(payload.search, "search", expectedPreOpenReadFailureCode);
  expectFailureEnvelope(payload.attachments, "attachments", expectedPreOpenReadFailureCode);
  expectFailureEnvelope(payload.attachmentGet, "attachmentGet", expectedPreOpenReadFailureCode);
  expectFailureEnvelope(payload.attachmentRefs, "attachmentRefs", expectedPreOpenReadFailureCode);
  expectFailureEnvelope(payload.attachmentReferrers, "attachmentReferrers", expectedPreOpenReadFailureCode);
  expectFailureEnvelope(payload.pdfMetadata, "pdfMetadata", expectedPreOpenReadFailureCode);
  expectFailureEnvelope(payload.pdfNoteRefs, "pdfNoteRefs", expectedPreOpenReadFailureCode);
  expectFailureEnvelope(payload.pdfReferrers, "pdfReferrers", expectedPreOpenReadFailureCode);
  expectFailureEnvelope(payload.chemMetadata, "chemMetadata", expectedPreOpenReadFailureCode);
  expectFailureEnvelope(payload.chemSpectra, "chemSpectra", expectedPreOpenReadFailureCode);
  expectFailureEnvelope(payload.chemSpectrum, "chemSpectrum", expectedPreOpenReadFailureCode);
  expectFailureEnvelope(payload.chemNoteRefs, "chemNoteRefs", expectedPreOpenReadFailureCode);
  expectFailureEnvelope(payload.chemReferrers, "chemReferrers", expectedPreOpenReadFailureCode);
  expectFailureEnvelope(payload.diagnosticsExport, "diagnosticsExport", expectedPreOpenReadFailureCode);
  expectFailureEnvelope(payload.rebuildStart, "rebuildStart", expectedPreOpenReadFailureCode);
  expectFailureEnvelope(payload.rebuildWait, "rebuildWait", expectedPreOpenReadFailureCode);
  expect(payload.bootstrap.data.security.contextIsolation === true, "contextIsolation should stay enabled.");
  expect(payload.bootstrap.data.security.nodeIntegration === false, "nodeIntegration should stay disabled.");
  expect(payload.bootstrap.data.security.enableRemoteModule === false, "remote module should stay disabled.");
  expect(payload.bootstrap.data.security.sandbox === true, "sandbox should stay enabled.");

  expectArrayIncludes(
    payload.bootstrap.data.api_groups,
    ["bootstrap", "runtime", "session", "search", "attachments", "pdf", "chemistry", "diagnostics", "rebuild"],
    "bootstrap api_groups"
  );

  expect(payload.runtime.data.lifecycle_state === "ready", "runtime lifecycle should be ready during smoke.");
  expect(typeof payload.runtime.data.kernel_binding.attached === "boolean", "kernel binding attached flag should be present.");
  expect(payload.runtime.data.rebuild.in_flight === false, "rebuild should not be in flight during baseline smoke.");
  expect(payload.session.data.state === "none", "session should begin closed.");
  expect(payload.openAttempt.data.result === "opened", "openAttempt should open the smoke vault.");
  expect(payload.runtimeAfterOpen.data.kernel_runtime.session_state === "open", "runtimeAfterOpen should report an open kernel session.");
  expect(payload.runtimeAfterOpen.data.kernel_runtime.index_state === "ready", "runtimeAfterOpen should observe the smoke vault in READY state.");
  expect(payload.rebuildStatusAfterOpen.data.adapterAttached === true, "rebuildStatusAfterOpen should stay attached after opening the vault.");
  expect(payload.rebuildStatusAfterOpen.data.status.indexState === "ready", "rebuildStatusAfterOpen should reflect READY after catch-up.");
  expect(payload.postOpenSession.data.state === "open", "postOpenSession should report an open host session.");
  expect(payload.postOpenSession.data.active_vault_path, "postOpenSession should expose the active vault path.");
  expect(payload.postOpenSession.data.last_error === null, "postOpenSession should clear the last error after a successful open.");
  expect(payload.searchAfterOpen.data.totalHits >= 1, "searchAfterOpen should return at least one hit.");
  expect(payload.searchAfterOpen.data.items[0].relPath === "notes/example.md", "searchAfterOpen should return the seeded note.");
  expect(payload.attachmentsAfterOpen.data.count >= 2, "attachmentsAfterOpen should project the seeded attachment catalog.");
  expectArrayIncludes(
    payload.attachmentsAfterOpen.data.items.map((item) => item.relPath),
    ["assets/sample.jdx", "assets/sample.pdf"],
    "attachmentsAfterOpen item relPaths"
  );
  expect(payload.attachmentGetAfterOpen.data.relPath === "assets/sample.pdf", "attachmentGetAfterOpen should resolve the seeded PDF attachment.");
  expectArrayIncludes(
    payload.attachmentRefsAfterOpen.data.items.map((item) => item.relPath),
    ["assets/sample.jdx", "assets/sample.pdf"],
    "attachmentRefsAfterOpen item relPaths"
  );
  expect(payload.attachmentReferrersAfterOpen.data.count === 1, "attachmentReferrersAfterOpen should resolve one referrer note.");
  expect(payload.attachmentReferrersAfterOpen.data.items[0].noteRelPath === "notes/example.md", "attachmentReferrersAfterOpen should resolve the seeded note path.");
  expect(payload.pdfMetadataAfterOpen.data.relPath === "assets/sample.pdf", "pdfMetadataAfterOpen should resolve the seeded PDF metadata.");
  expect(payload.pdfMetadataAfterOpen.data.pageCount >= 1, "pdfMetadataAfterOpen should expose a positive page count.");
  expect(payload.pdfNoteRefsAfterOpen.data.count === 0, "pdfNoteRefsAfterOpen should stay empty without formal #anchor refs.");
  expect(payload.pdfReferrersAfterOpen.data.count === 0, "pdfReferrersAfterOpen should stay empty without formal PDF source refs.");
  expect(payload.chemMetadataAfterOpen.data.count > 0, "chemMetadataAfterOpen should expose chemistry metadata entries.");
  expect(payload.chemSpectraAfterOpen.data.count >= 1, "chemSpectraAfterOpen should list the seeded spectrum.");
  expect(payload.chemSpectrumAfterOpen.data.attachmentRelPath === "assets/sample.jdx", "chemSpectrumAfterOpen should resolve the seeded spectrum carrier.");
  expect(payload.chemNoteRefsAfterOpen.data.count === 0, "chemNoteRefsAfterOpen should stay empty without formal #chemsel refs.");
  expect(payload.chemReferrersAfterOpen.data.count === 0, "chemReferrersAfterOpen should stay empty without formal chemistry refs.");
  expect(payload.diagnosticsExportAfterOpen.data.result === "exported", "diagnosticsExportAfterOpen should export a support bundle.");
  expect(
    typeof payload.diagnosticsExportAfterOpen.data.outputPath === "string" &&
      fs.existsSync(payload.diagnosticsExportAfterOpen.data.outputPath),
    "diagnosticsExportAfterOpen should materialize the support bundle file."
  );
  expect(payload.rebuildStartAfterOpen.data.result === "started", "rebuildStartAfterOpen should start a real rebuild.");
  expect(payload.rebuildWaitAfterOpen.data.result === "completed", "rebuildWaitAfterOpen should observe rebuild completion.");
  expect(payload.rebuildStatusAfterWait.data.status.hasLastResult === true, "rebuildStatusAfterWait should expose a completed rebuild result.");
  expect(payload.rebuildStatusAfterWait.data.status.lastResultCode === "KERNEL_OK", "rebuildStatusAfterWait should expose a successful rebuild result.");
  expect(payload.rebuildStatusAfterWait.data.status.indexState === "ready", "rebuildStatusAfterWait should return the host to READY.");
  expect(payload.closeAttempt.data.result === "closed", "closeAttempt should close the smoke vault.");
  expect(payload.rebuildStatus.data.adapterAttached === payload.runtime.data.kernel_binding.attached, "rebuild status should reflect the adapter attachment flag.");
  expect(payload.search.request_id === "smoke-search", "search request_id should round-trip.");
  expect(payload.searchAfterOpen.request_id === "smoke-search-after-open", "searchAfterOpen request_id should round-trip.");
}

module.exports = {
  assertSmokePayload
};
