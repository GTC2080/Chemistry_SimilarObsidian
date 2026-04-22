/**
 * note-chem-refs.js
 *
 * Reusable component: given a noteRelPath, renders chemistry spectrum links with ref type labels.
 */

import { chemistry } from "../../services/host-api-client.js";
import { createStateSurface } from "../../components/shared/state-surface.js";
import { mapChemNoteRef } from "./spectrum-view-model.js";

export function createNoteChemRefs(noteRelPath, opts = {}) {
  const { onSelect } = opts;
  const container = document.createElement("div");
  container.className = "note-chem-refs";

  async function load() {
    const env = await chemistry.queryNoteRefs({ noteRelPath, limit: 64 }, "app-note-chem-refs");
    container.innerHTML = "";

    if (!env.ok) {
      if (env.error?.code === "HOST_KERNEL_SURFACE_NOT_INTEGRATED") {
        const empty = document.createElement("div");
        empty.style.cssText = "font-size: 12px; color: #6b7280;";
        empty.textContent = "Chemistry features are not available in this build.";
        container.appendChild(empty);
        return;
      }
      container.appendChild(createStateSurface("error", { error: env.error, onRetry: load }));
      return;
    }

    const items = env.data?.items ?? [];
    if (items.length === 0) {
      const empty = document.createElement("div");
      empty.style.cssText = "font-size: 12px; color: #6b7280;";
      empty.textContent = "No chemistry references.";
      container.appendChild(empty);
      return;
    }

    for (const raw of items) {
      const ref = mapChemNoteRef(raw);
      const row = document.createElement("div");
      row.style.cssText = "display: flex; align-items: center; gap: 8px; padding: 4px 0;";

      const link = document.createElement("button");
      link.textContent = ref.noteRelPath || noteRelPath;
      link.style.cssText = `
        border: none;
        background: transparent;
        color: #2563eb;
        cursor: pointer;
        font-size: 13px;
        text-align: left;
      `;
      link.addEventListener("click", () => onSelect?.(ref.noteRelPath));
      row.appendChild(link);

      const kindBadge = document.createElement("span");
      kindBadge.style.cssText = `
        padding: 1px 6px;
        border-radius: 4px;
        background: #e0e7ff;
        color: #3730a3;
        font-size: 11px;
        text-transform: capitalize;
      `;
      kindBadge.textContent = ref.refKind === "x_range" ? "x-range" : "whole-spectrum";
      row.appendChild(kindBadge);

      if (ref.refKind === "x_range" && (ref.xMin != null || ref.xMax != null)) {
        const range = document.createElement("span");
        range.style.cssText = "color: #6b7280; font-size: 12px;";
        range.textContent = `${ref.xMin ?? ""} – ${ref.xMax ?? ""}`;
        row.appendChild(range);
      }

      container.appendChild(row);
    }
  }

  load();
  return container;
}
