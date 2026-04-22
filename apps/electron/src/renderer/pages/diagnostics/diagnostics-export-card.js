/**
 * diagnostics-export-card.js
 *
 * Export support bundle workflow.
 * Uses plain text input for outputPath as temporary measure
 * while EXPLICIT-HOST-GAP-010 (save dialog) remains unfilled.
 */

import { diagnostics } from "../../services/host-api-client.js";
import { createStateSurface } from "../../components/shared/state-surface.js";

export function createDiagnosticsExportCard() {
  const card = document.createElement("div");
  card.className = "diagnostics-export-card";
  card.style.cssText = `
    padding: 18px;
    border-radius: 20px;
    background: rgba(29, 28, 35, 0.94);
    border: 1px solid rgba(255,255,255,0.06);
    margin-bottom: 16px;
  `;

  const heading = document.createElement("div");
  heading.style.cssText = "font-weight: 600; font-size: 14px; margin-bottom: 12px; color:#f5f3ff;";
  heading.textContent = "导出 Support Bundle";
  card.appendChild(heading);

  const inputWrap = document.createElement("div");
  inputWrap.style.cssText = "display: flex; gap: 8px; margin-bottom: 10px;";

  const input = document.createElement("input");
  input.type = "text";
  input.placeholder = "/path/to/support-bundle.zip";
  input.style.cssText = `
    flex: 1;
    padding: 10px 12px;
    border-radius: 12px;
    border: 1px solid rgba(255,255,255,0.08);
    font-size: 13px;
    background: rgba(255,255,255,0.04);
    color: #f5f3ff;
  `;
  inputWrap.appendChild(input);

  const btn = document.createElement("button");
  btn.textContent = "Export";
  btn.style.cssText = `
    padding: 10px 18px;
    border-radius: 12px;
    border: none;
    background: linear-gradient(180deg, #8b5cf6, #6d28d9);
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
      padding: 12px;
      border-radius: 14px;
      background: rgba(22, 101, 52, 0.32);
      border: 1px solid rgba(34,197,94,0.22);
      color: #86efac;
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
