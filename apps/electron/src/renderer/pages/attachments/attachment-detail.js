/**
 * attachment-detail.js
 *
 * Single attachment view with metadata, PDF subsection, and referrers.
 */

import { attachments, pdf } from "../../services/host-api-client.js";
import { createPdfMetadataCard } from "./pdf-metadata-card.js";
import { mapAttachmentRecord, mapPdfMetadata } from "./attachment-view-model.js";
import { createStateSurface } from "../../components/shared/state-surface.js";

export function createAttachmentDetail(relPath) {
  const page = document.createElement("div");
  page.className = "attachment-detail";

  const header = document.createElement("div");
  header.style.cssText = `
    display: flex;
    align-items: center;
    gap: 10px;
    margin-bottom: 16px;
  `;

  const backBtn = document.createElement("button");
  backBtn.textContent = "← Back";
  backBtn.style.cssText = `
    padding: 8px 14px;
    border-radius: 10px;
    border: 1px solid rgba(255,255,255,0.08);
    background: rgba(255,255,255,0.04);
    color: #ede9fe;
    cursor: pointer;
    font-size: 12px;
  `;
  header.appendChild(backBtn);

  const title = document.createElement("h3");
  title.style.cssText = "margin: 0; font-size: 16px; color: #f5f3ff;";
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
      padding: 18px;
      border-radius: 20px;
      background: rgba(29, 28, 35, 0.94);
      border: 1px solid rgba(255,255,255,0.06);
      margin-bottom: 14px;
      font-size: 13px;
      color: #ece7ff;
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
      wrap.style.cssText = "display: flex; gap: 8px; padding: 5px 0;";
      const lbl = document.createElement("span");
      lbl.style.cssText = "color: #968ead; min-width: 80px;";
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
    refsSection.style.cssText = "margin-top: 18px;";
    const refsHeading = document.createElement("div");
    refsHeading.style.cssText = "font-weight: 600; font-size: 13px; margin-bottom: 10px; color:#f5f3ff;";
    refsHeading.textContent = "引用该附件的笔记";
    refsSection.appendChild(refsHeading);

    const refsEnv = await attachments.queryReferrers({ attachmentRelPath: relPath, limit: 64 }, "app-attachment-refs");
    if (refsEnv.ok && refsEnv.data?.items?.length > 0) {
      for (const ref of refsEnv.data.items) {
        const row = document.createElement("div");
        row.style.cssText = "padding: 9px 0; font-size: 13px; border-bottom: 1px solid rgba(255,255,255,0.06); color:#d8d1ec;";
        row.textContent = ref.noteRelPath ?? ref.note_rel_path ?? JSON.stringify(ref);
        refsSection.appendChild(row);
      }
    } else {
      const none = document.createElement("div");
      none.style.cssText = "font-size: 13px; color: #968ead;";
      none.textContent = "暂无引用该附件的笔记。";
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
    padding: 12px 14px;
    margin-bottom: 12px;
    border-radius: 14px;
    background: ${bg === "#fee2e2" ? "rgba(127, 29, 29, 0.36)" : "rgba(120, 53, 15, 0.34)"};
    border: 1px solid ${bg === "#fee2e2" ? "rgba(248,113,113,0.22)" : "rgba(250,204,21,0.2)"};
    color: ${bg === "#fee2e2" ? "#fca5a5" : "#fde68a"};
    font-size: 13px;
  `;
  el.textContent = text;
  return el;
}
