/**
 * attachment-detail.js
 *
 * Single attachment view with metadata, PDF subsection, and referrers.
 */

import { attachments, pdf } from "../services/host-api-client.js";
import { createPdfMetadataCard } from "./pdf-metadata-card.js";
import { mapAttachmentRecord, mapPdfMetadata } from "./attachment-view-model.js";
import { createStateSurface } from "../components/shared/state-surface.js";

export function createAttachmentDetail(relPath) {
  const page = document.createElement("div");
  page.className = "attachment-detail";

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
  title.textContent = relPath;
  header.appendChild(title);

  page.appendChild(header);

  const content = document.createElement("div");
  content.className = "attachment-detail-content";
  page.appendChild(content);

  async function load() {
    content.innerHTML = "";
    content.appendChild(createStateSurface("loading", { loadingLabel: "Loading attachment..." }));

    const [attachEnv, pdfEnv] = await Promise.all([
      attachments.get({ attachmentRelPath: relPath }, "app-attachment-get"),
      pdf.getMetadata({ attachmentRelPath: relPath }, "app-pdf-meta")
    ]);

    content.innerHTML = "";

    if (!attachEnv.ok) {
      content.appendChild(createStateSurface("error", {
        error: attachEnv.error,
        onRetry: load
      }));
      return;
    }

    const record = mapAttachmentRecord(attachEnv.data);

    // State banner
    if (record.state === "missing") {
      content.appendChild(banner("Attachment file is missing on disk.", "#fee2e2", "#991b1b"));
    } else if (record.state === "stale") {
      content.appendChild(banner("Attachment metadata may be out of date.", "#fef9c3", "#854d0e"));
    }

    // Metadata card
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
      { label: "Path", value: record.relPath },
      { label: "Name", value: record.fileName },
      { label: "Extension", value: record.extension || "—" },
      { label: "Size", value: record.sizeBytes != null ? `${record.sizeBytes} bytes` : "—" },
      { label: "Modified", value: record.modifiedAt || "—" },
      { label: "State", value: record.state }
    ];

    for (const row of metaRows) {
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

    // PDF metadata if applicable
    if (pdfEnv.ok && pdfEnv.data) {
      const pdfMeta = mapPdfMetadata(pdfEnv.data);
      content.appendChild(createPdfMetadataCard(pdfMeta));
    }

    // Referrers
    const refsSection = document.createElement("div");
    refsSection.style.cssText = "margin-top: 16px;";
    const refsHeading = document.createElement("div");
    refsHeading.style.cssText = "font-weight: 600; font-size: 13px; margin-bottom: 8px;";
    refsHeading.textContent = "Referrers";
    refsSection.appendChild(refsHeading);

    const refsEnv = await attachments.queryReferrers({ attachmentRelPath: relPath, limit: 64 }, "app-attachment-refs");
    if (refsEnv.ok && refsEnv.data?.items?.length > 0) {
      for (const ref of refsEnv.data.items) {
        const row = document.createElement("div");
        row.style.cssText = "padding: 6px 0; font-size: 13px; border-bottom: 1px solid #f3f4f6;";
        row.textContent = ref.noteRelPath ?? ref.note_rel_path ?? JSON.stringify(ref);
        refsSection.appendChild(row);
      }
    } else {
      const none = document.createElement("div");
      none.style.cssText = "font-size: 13px; color: #6b7280;";
      none.textContent = "No referring notes.";
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
