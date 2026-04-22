/**
 * pdf-metadata-card.js
 *
 * PDF-specific metadata display.
 */

export function createPdfMetadataCard(metadata) {
  const card = document.createElement("div");
  card.className = "pdf-metadata-card";
  card.style.cssText = `
    padding: 14px;
    border-radius: 8px;
    background: #eff6ff;
    border: 1px solid #bfdbfe;
    margin-top: 12px;
  `;

  const heading = document.createElement("div");
  heading.style.cssText = "font-weight: 600; font-size: 13px; margin-bottom: 8px; color: #1e3a8a;";
  heading.textContent = "PDF Metadata";
  card.appendChild(heading);

  if (!metadata) {
    const empty = document.createElement("div");
    empty.style.cssText = "font-size: 12px; color: #6b7280;";
    empty.textContent = "No PDF metadata available.";
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
    wrap.style.cssText = "display: flex; gap: 8px; font-size: 12px; padding: 2px 0;";

    const lbl = document.createElement("span");
    lbl.style.color = "#6b7280";
    lbl.style.minWidth = "60px";
    lbl.textContent = row.label;
    wrap.appendChild(lbl);

    const val = document.createElement("span");
    val.style.color = "#111827";
    val.textContent = row.value;
    wrap.appendChild(val);

    card.appendChild(wrap);
  }

  return card;
}
