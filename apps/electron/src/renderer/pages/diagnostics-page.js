/**
 * diagnostics-page.js
 *
 * Diagnostics and rebuild control center.
 */

import { createDiagnosticsExportCard } from "./diagnostics-export-card.js";
import { createRebuildControlCard } from "./rebuild-control-card.js";
import { createRuntimePage } from "./runtime-page.js";
import { store } from "../state/host-store.js";

export function createDiagnosticsPage() {
  const page = document.createElement("div");
  page.className = "diagnostics-page";
  page.style.cssText = "max-width: 720px;";

  const exportCard = createDiagnosticsExportCard();
  page.appendChild(exportCard);

  const rebuildCard = createRebuildControlCard();
  page.appendChild(rebuildCard);

  const runtimeHeading = document.createElement("div");
  runtimeHeading.style.cssText = "font-weight: 600; font-size: 14px; margin-bottom: 10px;";
  runtimeHeading.textContent = "Runtime Summary";
  page.appendChild(runtimeHeading);

  const runtimeView = createRuntimePage(store.getRuntimeSummary());
  page.appendChild(runtimeView);

  return page;
}
