/**
 * note-attachment-refs.js
 *
 * Reusable component: given a noteRelPath, renders attachment links.
 */

import { attachments } from "../services/host-api-client.js";
import { createStateSurface } from "../components/shared/state-surface.js";

export function createNoteAttachmentRefs(noteRelPath, opts = {}) {
  const { onSelect } = opts;
  const container = document.createElement("div");
  container.className = "note-attachment-refs";

  async function load() {
    const env = await attachments.queryNoteRefs({ noteRelPath, limit: 64 }, "app-note-attach-refs");
    container.innerHTML = "";

    if (!env.ok) {
      container.appendChild(createStateSurface("error", {
        error: env.error,
        onRetry: load
      }));
      return;
    }

    const items = env.data?.items ?? [];
    if (items.length === 0) {
      const empty = document.createElement("div");
      empty.style.cssText = "font-size: 12px; color: #6b7280;";
      empty.textContent = "No attachments referenced.";
      container.appendChild(empty);
      return;
    }

    for (const item of items) {
      const path = item.relPath ?? item.rel_path ?? "";
      const link = document.createElement("button");
      link.textContent = path;
      link.style.cssText = `
        display: block;
        padding: 4px 0;
        border: none;
        background: transparent;
        color: #2563eb;
        cursor: pointer;
        font-size: 13px;
        text-align: left;
      `;
      link.addEventListener("click", () => onSelect?.(path));
      container.appendChild(link);
    }
  }

  load();
  return container;
}
