/**
 * runtime-page.js
 *
 * Diagnostic view: formatted runtime.getSummary() dump.
 */

import { createStateSurface } from "../components/shared/state-surface.js";

export function createRuntimePage(runtimeEnvelope) {
  const page = document.createElement("div");
  page.className = "runtime-page";

  if (!runtimeEnvelope || !runtimeEnvelope.ok) {
    page.appendChild(createStateSurface("error", {
      error: runtimeEnvelope?.error ?? { code: "NO_DATA", message: "No runtime data available." }
    }));
    return page;
  }

  const data = runtimeEnvelope.data;

  const sections = [
    { key: "lifecycle_state", label: "Lifecycle" },
    { key: "run_mode", label: "Run Mode" },
    { key: "main_window", label: "Main Window" },
    { key: "kernel_runtime", label: "Kernel Runtime" },
    { key: "rebuild", label: "Rebuild" },
    { key: "session", label: "Session" },
    { key: "kernel_binding", label: "Kernel Binding" },
    { key: "last_window_event", label: "Last Window Event" }
  ];

  for (const section of sections) {
    const block = document.createElement("div");
    block.style.cssText = `
      margin-bottom: 12px;
      padding: 12px;
      border-radius: 6px;
      background: #fff;
      border: 1px solid #e5e7eb;
    `;

    const title = document.createElement("div");
    title.style.cssText = "font-weight: 600; font-size: 13px; margin-bottom: 6px; color: #374151;";
    title.textContent = section.label;
    block.appendChild(title);

    const pre = document.createElement("pre");
    pre.style.cssText = `
      margin: 0;
      font-size: 12px;
      background: #f9fafb;
      padding: 8px;
      border-radius: 4px;
      overflow: auto;
      color: #4b5563;
    `;
    pre.textContent = JSON.stringify(data[section.key] ?? null, null, 2);
    block.appendChild(pre);

    page.appendChild(block);
  }

  return page;
}
