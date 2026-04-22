/**
 * pdf-metadata-card.js
 *
 * PDF-specific metadata display.
 */

export function createPdfMetadataCard(metadata) {
  const card = document.createElement("div");
  card.className = "pdf-metadata-card";
  card.style.cssText = `
    padding: 16px;
    border-radius: 18px;
    background: rgba(37, 31, 71, 0.55);
    border: 1px solid rgba(167, 139, 250, 0.28);
    margin-top: 12px;
  `;

  const heading = document.createElement("div");
  heading.style.cssText = "font-weight: 600; font-size: 13px; margin-bottom: 10px; color: #ddd6fe;";
  heading.textContent = "PDF 元数据";
  card.appendChild(heading);

  if (!metadata) {
    const empty = document.createElement("div");
    empty.style.cssText = "font-size: 12px; color: #a59fc0;";
    empty.textContent = "当前没有可用的 PDF 元数据。";
    card.appendChild(empty);
    return card;
  }

  const rows = [
    { label: "Title", value: metadata.title },
    { label: "Author", value: metadata.author },
    { label: "Pages", value: metadata.pageCount != null ? String(metadata.pageCount) : null },
    { label: "Created", value: metadata.creationDate }
  ];

  for (const row of rows) {
    if (row.value == null) continue;
    const wrap = document.createElement("div");
    wrap.style.cssText = "display: flex; gap: 8px; font-size: 12px; padding: 4px 0;";

    const lbl = document.createElement("span");
    lbl.style.color = "#a59fc0";
    lbl.style.minWidth = "60px";
    lbl.textContent = row.label;
    wrap.appendChild(lbl);

    const val = document.createElement("span");
    val.style.color = "#f5f3ff";
    val.textContent = row.value;
    wrap.appendChild(val);

    card.appendChild(wrap);
  }

  return card;
}
