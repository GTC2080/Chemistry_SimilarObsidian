/**
 * diagnostics-export-card.js
 *
 * Export support bundle workflow.
 * Uses plain text input for outputPath as temporary measure
 * while EXPLICIT-HOST-GAP-010 (save dialog) remains unfilled.
 */

import { diagnostics } from "../services/host-api-client.js";
import { createStateSurface } from "../components/shared/state-surface.js";

export function createDiagnosticsExportCard() {
  const card = document.createElement("div");
  card.className = "diagnostics-export-card";
  card.style.cssText = `
    padding: 16px;
    border-radius: 8px;
    background: #fff;
    border: 1px solid #e5e7eb;
    margin-bottom: 16px;
  `;

  const heading = document.createElement("div");
  heading.style.cssText = "font-weight: 600; font-size: 14px; margin-bottom: 12px;";
  heading.textContent = "Export Support Bundle";
  card.appendChild(heading);

  const inputWrap = document.createElement("div");
  inputWrap.style.cssText = "display: flex; gap: 8px; margin-bottom: 10px;";

  const input = document.createElement("input");
  input.type = "text";
  input.placeholder = "/path/to/support-bundle.zip";
  input.style.cssText = `
    flex: 1;
    padding: 8px 12px;
    border-radius: 6px;
    border: 1px solid #d1d5db;
    font-size: 13px;
  `;
  inputWrap.appendChild(input);

  const btn = document.createElement("button");
  btn.textContent = "Export";
  btn.style.cssText = `
    padding: 8px 16px;
    border-radius: 6px;
    border: none;
    background: #2563eb;
    color: #fff;
    cursor: pointer;
    font-size: 13px;
  `;
  inputWrap.appendChild(btn);
  card.appendChild(inputWrap);

  const statusArea = document.createElement("div");
  card.appendChild(statusArea);

  async function doExport() {
    const outputPath = input.value.trim();
    if (!outputPath) {
      input.style.borderColor = "#ef4444";
      return;
    }
    input.style.borderColor = "#d1d5db";
    statusArea.innerHTML = "";
    statusArea.appendChild(createStateSurface("loading", { loadingLabel: "Exporting..." }));

    const env = await diagnostics.exportSupportBundle({ outputPath }, "app-diagnostics-export");
    statusArea.innerHTML = "";

    if (!env.ok) {
      if (env.error?.code === "HOST_INVALID_ARGUMENT") {
        input.style.borderColor = "#ef4444";
      }
      statusArea.appendChild(createStateSurface("error", {
        error: env.error,
        onRetry: doExport
      }));
      return;
    }

    const result = env.data;
    const success = document.createElement("div");
    success.style.cssText = `
      padding: 10px;
      border-radius: 6px;
      background: #dcfce7;
      color: #166534;
      font-size: 13px;
    `;
    const path = result?.outputPath ?? outputPath;
    success.textContent = `Exported to: ${path}`;
    statusArea.appendChild(success);
  }

  btn.addEventListener("click", doExport);
  input.addEventListener("keydown", (e) => {
    if (e.key === "Enter") doExport();
  });

  return card;
}
