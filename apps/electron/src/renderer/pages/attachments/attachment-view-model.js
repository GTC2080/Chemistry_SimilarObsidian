/**
 * attachment-view-model.js
 *
 * Mapper for attachment / PDF envelopes.
 */

import { reportHostGap } from "../../services/envelope-guard.js";

export function mapAttachmentRecord(data) {
  if (!data || typeof data !== "object") {
    return { relPath: "", fileName: "", extension: "", sizeBytes: null, modifiedAt: null, state: "unresolved" };
  }

  const required = ["relPath"];
  for (const field of required) {
    if (!(field in data)) {
      reportHostGap("EXPLICIT-HOST-GAP-004", `Attachment record missing required field: ${field}`);
    }
  }

  return {
    relPath: data.relPath ?? data.rel_path ?? "",
    fileName: data.fileName ?? data.file_name ?? "",
    extension: data.extension ?? "",
    sizeBytes: data.sizeBytes ?? data.size_bytes ?? null,
    modifiedAt: data.modifiedAt ?? data.modified_at ?? null,
    state: normalizeAttachmentState(data.state)
  };
}

export function mapAttachmentList(data) {
  if (!data || typeof data !== "object") {
    return { items: [], count: 0 };
  }
  const items = Array.isArray(data.items) ? data.items.map(mapAttachmentRecord) : [];
  return { items, count: typeof data.count === "number" ? data.count : items.length };
}

export function mapPdfMetadata(data) {
  if (!data || typeof data !== "object") {
    return null;
  }
  const required = ["relPath"];
  for (const field of required) {
    if (!(field in data)) {
      reportHostGap("EXPLICIT-HOST-GAP-005", `PDF metadata missing required field: ${field}`);
    }
  }
  return {
    relPath: data.relPath ?? data.rel_path ?? "",
    title: data.title ?? null,
    author: data.author ?? null,
    pageCount: data.pageCount ?? data.page_count ?? null,
    creationDate: data.creationDate ?? data.creation_date ?? null
  };
}

function normalizeAttachmentState(state) {
  const valid = ["present", "missing", "stale", "unresolved"];
  if (valid.includes(state)) return state;
  return "unresolved";
}
