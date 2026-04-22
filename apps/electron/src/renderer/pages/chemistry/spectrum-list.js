/**
 * spectrum-list.js
 *
 * Table of chemistry spectra.
 */

import { createStateSurface } from "../../components/shared/state-surface.js";

export function createSpectrumList(viewModel, opts = {}) {
  const { onSelect } = opts;
  const container = document.createElement("div");
  container.className = "spectrum-list";

  if (viewModel.items.length === 0) {
    container.appendChild(createStateSurface("empty", { emptyMessage: "No spectra found." }));
    return container;
  }

  const table = document.createElement("table");
  table.style.cssText = `
    width: 100%;
    border-collapse: collapse;
    font-size: 13px;
    background: rgba(29, 28, 35, 0.94);
    border: 1px solid rgba(255,255,255,0.06);
    border-radius: 18px;
    overflow: hidden;
    color: #ece7ff;
  `;

  const thead = document.createElement("thead");
  thead.style.cssText = "background: rgba(255,255,255,0.03); color: #938bab; text-align: left;";
  const headerRow = document.createElement("tr");
  const headers = ["Title", "Data Type", "X Units", "Y Units", "Source"];
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
    tr.style.cssText = "border-top: 1px solid rgba(255,255,255,0.06); cursor: pointer;";
    tr.addEventListener("mouseenter", () => { tr.style.background = "rgba(255,255,255,0.04)"; });
    tr.addEventListener("mouseleave", () => { tr.style.background = "transparent"; });
    tr.addEventListener("click", () => onSelect?.(item.attachmentRelPath));

    const cells = [
      item.title || "Untitled",
      item.dataType || "—",
      item.xUnits || "—",
      item.yUnits || "—",
      item.attachmentRelPath || "—"
    ];

    for (const text of cells) {
      const td = document.createElement("td");
      td.style.cssText = "padding: 12px 14px; color: #ece7ff;";
      td.textContent = text;
      tr.appendChild(td);
    }

    tbody.appendChild(tr);
  }
  table.appendChild(tbody);
  container.appendChild(table);

  return container;
}
