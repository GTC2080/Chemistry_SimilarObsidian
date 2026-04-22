/**
 * spectrum-detail.js
 *
 * Single spectrum view with metadata, note refs, and ref type indicators.
 */

import { chemistry } from "../services/host-api-client.js";
import { mapSpectrumRecord, mapChemNoteRef } from "./spectrum-view-model.js";
import { createStateSurface } from "../components/shared/state-surface.js";

export function createSpectrumDetail(attachmentRelPath) {
  const page = document.createElement("div");
  page.className = "spectrum-detail";

  const header = document.createElement("div");
  header.style.cssText = `
    display: flex;
    align-items: center;
    gap: 10px;
    margin-bottom: 12px;
  `;

  const backBtn = document.createElement("button");
  backBtn.textContent = "← Back";
  backBtn.style.cssText = `
    padding: 5px 10px;
    border-radius: 6px;
    border: 1px solid #d1d5db;
    background: #fff;
    cursor: pointer;
    font-size: 12px;
  `;
  header.appendChild(backBtn);

  const title = document.createElement("h3");
  title.style.cssText = "margin: 0; font-size: 16px;";
  title.textContent = attachmentRelPath;
  header.appendChild(title);

  page.appendChild(header);

  const content = document.createElement("div");
  content.className = "spectrum-detail-content";
  page.appendChild(content);

  async function load() {
    content.innerHTML = "";
    content.appendChild(createStateSurface("loading", { loadingLabel: "Loading spectrum..." }));

    const env = await chemistry.getSpectrum({ attachmentRelPath }, "app-spectrum-get");
    content.innerHTML = "";

    if (!env.ok) {
      if (env.error?.code === "HOST_KERNEL_SURFACE_NOT_INTEGRATED") {
        content.appendChild(createStateSurface("empty", {
          emptyMessage: "Chemistry features are not available in this build."
        }));
        return;
      }
      content.appendChild(createStateSurface("error", { error: env.error, onRetry: load }));
      return;
    }

    const record = mapSpectrumRecord(env.data);

    if (record.state === "unsupported") {
      content.appendChild(banner("Spectrum format not supported for display.", "#fee2e2", "#991b1b"));
    } else if (record.state === "missing") {
      content.appendChild(banner("Source file missing. Spectrum data unavailable.", "#fee2e2", "#991b1b"));
    }

    const metaCard = document.createElement("div");
    metaCard.style.cssText = `
      padding: 14px;
      border-radius: 8px;
      background: #fff;
      border: 1px solid #e5e7eb;
      margin-bottom: 12px;
      font-size: 13px;
    `;

    const metaRows = [
      { label: "Title", value: record.title },
      { label: "Data Type", value: record.dataType },
      { label: "X Units", value: record.xUnits },
      { label: "Y Units", value: record.yUnits },
      { label: "Points", value: record.nPoints != null ? String(record.nPoints) : null },
      { label: "State", value: record.state }
    ];

    for (const row of metaRows) {
      if (row.value == null) continue;
      const wrap = document.createElement("div");
      wrap.style.cssText = "display: flex; gap: 8px; padding: 3px 0;";
      const lbl = document.createElement("span");
      lbl.style.cssText = "color: #6b7280; min-width: 80px;";
      lbl.textContent = row.label;
      wrap.appendChild(lbl);
      const val = document.createElement("span");
      val.textContent = row.value;
      wrap.appendChild(val);
      metaCard.appendChild(wrap);
    }
    content.appendChild(metaCard);

    // Source attachment link
    if (record.attachmentRelPath) {
      const sourceWrap = document.createElement("div");
      sourceWrap.style.cssText = "margin-bottom: 12px; font-size: 13px;";
      sourceWrap.innerHTML = `<span style="color:#6b7280;">Source:</span> ${record.attachmentRelPath}`;
      content.appendChild(sourceWrap);
    }

    // Note refs
    const refsSection = document.createElement("div");
    refsSection.style.cssText = "margin-top: 16px;";
    const refsHeading = document.createElement("div");
    refsHeading.style.cssText = "font-weight: 600; font-size: 13px; margin-bottom: 8px;";
    refsHeading.textContent = "Note References";
    refsSection.appendChild(refsHeading);

    const refsEnv = await chemistry.queryReferrers({ attachmentRelPath, limit: 64 }, "app-spectrum-refs");
    if (refsEnv.ok && refsEnv.data?.items?.length > 0) {
      for (const raw of refsEnv.data.items) {
        const ref = mapChemNoteRef(raw);
        const row = document.createElement("div");
        row.style.cssText = "padding: 6px 0; font-size: 13px; border-bottom: 1px solid #f3f4f6; display: flex; gap: 10px; align-items: center;";

        const path = document.createElement("span");
        path.textContent = ref.noteRelPath;
        row.appendChild(path);

        const kindBadge = document.createElement("span");
        kindBadge.style.cssText = `
          padding: 1px 6px;
          border-radius: 4px;
          background: #e0e7ff;
          color: #3730a3;
          font-size: 11px;
          text-transform: capitalize;
        `;
        const kindLabel = ref.refKind === "x_range" ? "x-range" : "whole-spectrum";
        kindBadge.textContent = kindLabel;
        row.appendChild(kindBadge);

        if (ref.refKind === "x_range" && (ref.xMin != null || ref.xMax != null)) {
          const range = document.createElement("span");
          range.style.cssText = "color: #6b7280; font-size: 12px;";
          range.textContent = `${ref.xMin ?? ""} – ${ref.xMax ?? ""}`;
          row.appendChild(range);
        }

        refsSection.appendChild(row);
      }
    } else {
      const none = document.createElement("div");
      none.style.cssText = "font-size: 13px; color: #6b7280;";
      none.textContent = "No note references.";
      refsSection.appendChild(none);
    }
    content.appendChild(refsSection);
  }

  load();
  return { element: page, backButton: backBtn };
}

function banner(text, bg, color) {
  const el = document.createElement("div");
  el.style.cssText = `
    padding: 10px 14px;
    margin-bottom: 12px;
    border-radius: 6px;
    background: ${bg};
    color: ${color};
    font-size: 13px;
  `;
  el.textContent = text;
  return el;
}
