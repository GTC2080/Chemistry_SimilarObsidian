/**
 * attachment-list.js
 *
 * Grid or table of attachments.
 */

import { createStateSurface } from "../components/shared/state-surface.js";

export function createAttachmentList(viewModel, opts = {}) {
  const { onSelect } = opts;
  const container = document.createElement("div");
  container.className = "attachment-list";

  if (viewModel.items.length === 0) {
    container.appendChild(createStateSurface("empty", { emptyMessage: "No attachments found." }));
    return container;
  }

  const table = document.createElement("table");
  table.style.cssText = `
    width: 100%;
    border-collapse: collapse;
    font-size: 13px;
    background: #fff;
    border: 1px solid #e5e7eb;
    border-radius: 6px;
    overflow: hidden;
  `;

  const thead = document.createElement("thead");
  thead.style.cssText = "background: #f9fafb; color: #6b7280; text-align: left;";
  const headerRow = document.createElement("tr");
  const headers = ["Path", "Kind", "Size", "State"];
  for (const h of headers) {
    const th = document.createElement("th");
    th.style.cssText = "padding: 10px 12px; font-weight: 500;";
    th.textContent = h;
    headerRow.appendChild(th);
  }
  thead.appendChild(headerRow);
  table.appendChild(thead);

  const tbody = document.createElement("tbody");
  for (const item of viewModel.items) {
    const tr = document.createElement("tr");
    tr.style.cssText = "border-top: 1px solid #f3f4f6; cursor: pointer;";
    tr.addEventListener("mouseenter", () => { tr.style.background = "#f9fafb"; });
    tr.addEventListener("mouseleave", () => { tr.style.background = "transparent"; });
    tr.addEventListener("click", () => onSelect?.(item.relPath));

    const tdPath = document.createElement("td");
    tdPath.style.cssText = "padding: 10px 12px; color: #111827;";
    tdPath.textContent = item.relPath;
    tr.appendChild(tdPath);

    const tdKind = document.createElement("td");
    tdKind.style.cssText = "padding: 10px 12px; text-transform: uppercase; font-size: 11px; color: #6b7280;";
    tdKind.textContent = item.extension || "—";
    tr.appendChild(tdKind);

    const tdSize = document.createElement("td");
    tdSize.style.cssText = "padding: 10px 12px; color: #6b7280;";
    tdSize.textContent = item.sizeBytes != null ? formatBytes(item.sizeBytes) : "—";
    tr.appendChild(tdSize);

    const tdState = document.createElement("td");
    tdState.style.cssText = "padding: 10px 12px;";
    tdState.appendChild(stateBadge(item.state));
    tr.appendChild(tdState);

    tbody.appendChild(tr);
  }
  table.appendChild(tbody);
  container.appendChild(table);

  return container;
}

function stateBadge(state) {
  const span = document.createElement("span");
  const colors = {
    present: { bg: "#dcfce7", text: "#166534", label: "Present" },
    missing: { bg: "#fee2e2", text: "#991b1b", label: "Missing" },
    stale: { bg: "#fef9c3", text: "#854d0e", label: "Stale" },
    unresolved: { bg: "#f3f4f6", text: "#4b5563", label: "Unresolved" }
  };
  const style = colors[state] || colors.unresolved;
  span.style.cssText = `
    display: inline-block;
    padding: 2px 8px;
    border-radius: 999px;
    font-size: 11px;
    background: ${style.bg};
    color: ${style.text};
  `;
  span.textContent = style.label;
  return span;
}

function formatBytes(bytes) {
  if (bytes === 0) return "0 B";
  const k = 1024;
  const sizes = ["B", "KB", "MB", "GB"];
  const i = Math.floor(Math.log(bytes) / Math.log(k));
  return parseFloat((bytes / Math.pow(k, i)).toFixed(1)) + " " + sizes[i];
}
