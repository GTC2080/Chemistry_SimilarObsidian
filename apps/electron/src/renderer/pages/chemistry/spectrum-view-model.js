/**
 * spectrum-view-model.js
 *
 * Mapper for chemistry spectra envelopes.
 */

import { reportHostGap } from "../../services/envelope-guard.js";

export function mapSpectrumRecord(data) {
  if (!data || typeof data !== "object") {
    return {
      attachmentRelPath: "",
      title: "",
      dataType: "",
      xUnits: "",
      yUnits: "",
      nPoints: null,
      state: "unresolved"
    };
  }

  const required = ["attachmentRelPath", "title", "dataType", "xUnits", "yUnits"];
  for (const field of required) {
    if (!(field in data)) {
      reportHostGap("EXPLICIT-HOST-GAP-007", `Spectrum record missing required field: ${field}`);
    }
  }

  return {
    attachmentRelPath: data.attachmentRelPath ?? data.attachment_rel_path ?? "",
    title: data.title ?? "",
    dataType: data.dataType ?? data.data_type ?? "",
    xUnits: data.xUnits ?? data.x_units ?? "",
    yUnits: data.yUnits ?? data.y_units ?? "",
    nPoints: data.nPoints ?? data.n_points ?? null,
    state: normalizeSpectrumState(data.state)
  };
}

export function mapSpectrumList(data) {
  if (!data || typeof data !== "object") {
    return { items: [], count: 0 };
  }
  const items = Array.isArray(data.items) ? data.items.map(mapSpectrumRecord) : [];
  return { items, count: typeof data.count === "number" ? data.count : items.length };
}

export function mapChemNoteRef(item) {
  if (!item || typeof item !== "object") {
    return { noteRelPath: "", refKind: "whole_spectrum", xMin: null, xMax: null };
  }

  const required = ["noteRelPath"];
  for (const field of required) {
    if (!(field in item)) {
      reportHostGap("EXPLICIT-HOST-GAP-008", `Chemistry note-ref missing required field: ${field}`);
    }
  }

  return {
    noteRelPath: item.noteRelPath ?? item.note_rel_path ?? "",
    refKind: item.refKind ?? item.ref_kind ?? "whole_spectrum",
    xMin: item.xMin ?? item.x_min ?? null,
    xMax: item.xMax ?? item.x_max ?? null
  };
}

function normalizeSpectrumState(state) {
  const valid = ["present", "missing", "unresolved", "unsupported"];
  if (valid.includes(state)) return state;
  return "unresolved";
}
