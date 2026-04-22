/**
 * current-note-view.js
 *
 * Minimal note reading surface for host-backed Files mode.
 */

export function createCurrentNoteView(opts = {}) {
  const {
    vaultName = "workspace",
    noteRecord = null,
    runtimeLine = "session=unknown · index=unknown"
  } = opts;

  const article = document.createElement("article");
  article.className = "current-note-view";
  article.style.cssText = `
    max-width: 920px;
    margin: 0 auto;
    padding: 28px 30px 40px;
  `;

  article.appendChild(createNoteHeader(vaultName, noteRecord, runtimeLine));

  const body = document.createElement("div");
  body.style.cssText = "display:grid; gap:14px;";
  for (const node of buildNoteBodyNodes(noteRecord?.bodyText || "")) {
    body.appendChild(node);
  }
  article.appendChild(body);

  return article;
}

function createNoteHeader(vaultName, noteRecord, runtimeLine) {
  const header = document.createElement("header");
  header.style.cssText = `
    margin-bottom: 28px;
    padding-bottom: 22px;
    border-bottom: 1px solid rgba(255,255,255,0.06);
  `;

  const breadcrumb = document.createElement("div");
  breadcrumb.style.cssText = `
    display:flex;
    align-items:center;
    gap:8px;
    flex-wrap:wrap;
    margin-bottom: 16px;
    color:#918aa6;
    font-size:12px;
  `;
  breadcrumb.appendChild(createMetaChip("Files"));
  breadcrumb.appendChild(createMetaChip(vaultName));
  breadcrumb.appendChild(createMetaChip(noteRecord?.relPath || "Current Note"));
  header.appendChild(breadcrumb);

  const title = document.createElement("div");
  title.style.cssText = "font-size: 36px; font-weight: 750; letter-spacing: -0.04em; color: #faf7ff;";
  title.textContent = noteRecord?.title || noteRecord?.name || "Untitled";
  header.appendChild(title);

  const meta = document.createElement("div");
  meta.style.cssText = "margin-top: 10px; font-size: 13px; color: #938ca9; line-height: 1.8;";
  meta.textContent = `${runtimeLine} · ${formatNoteMeta(noteRecord)}`;
  header.appendChild(meta);

  return header;
}

function buildNoteBodyNodes(bodyText) {
  const nodes = [];
  const lines = String(bodyText).replace(/\r\n/g, "\n").split("\n");
  let paragraphLines = [];
  let listItems = [];

  function flushParagraph() {
    if (paragraphLines.length === 0) {
      return;
    }
    const paragraph = document.createElement("p");
    paragraph.style.cssText = `
      margin: 0;
      color: #ddd6f4;
      line-height: 1.95;
      font-size: 15px;
      white-space: pre-wrap;
    `;
    paragraph.textContent = paragraphLines.join(" ");
    nodes.push(paragraph);
    paragraphLines = [];
  }

  function flushList() {
    if (listItems.length === 0) {
      return;
    }
    const list = document.createElement("div");
    list.style.cssText = "display:grid; gap:10px;";
    for (const itemText of listItems) {
      const row = document.createElement("div");
      row.style.cssText = `
        display:flex;
        align-items:flex-start;
        gap:10px;
        min-height: 42px;
        padding: 10px 14px;
        border-radius: 14px;
        border: 1px solid rgba(255,255,255,0.05);
        background: rgba(255,255,255,0.03);
        color: #d8d1ec;
        font-size: 14px;
        line-height: 1.75;
      `;

      const bullet = document.createElement("span");
      bullet.style.cssText = `
        width: 20px;
        height: 20px;
        border-radius: 999px;
        display:grid;
        place-items:center;
        background: rgba(139,92,246,0.16);
        color: #d8b4fe;
        font-size: 12px;
        flex-shrink: 0;
        margin-top: 1px;
      `;
      bullet.textContent = "•";
      row.appendChild(bullet);

      const label = document.createElement("div");
      label.textContent = itemText;
      row.appendChild(label);
      list.appendChild(row);
    }
    nodes.push(list);
    listItems = [];
  }

  for (const line of lines) {
    const trimmed = line.trim();

    if (!trimmed) {
      flushParagraph();
      flushList();
      continue;
    }

    const headingMatch = trimmed.match(/^(#{1,6})\s+(.+)$/);
    if (headingMatch) {
      flushParagraph();
      flushList();
      nodes.push(createHeadingNode(headingMatch[2], headingMatch[1].length));
      continue;
    }

    const listMatch = trimmed.match(/^[-*]\s+(.+)$/);
    if (listMatch) {
      flushParagraph();
      listItems.push(listMatch[1]);
      continue;
    }

    flushList();
    paragraphLines.push(trimmed);
  }

  flushParagraph();
  flushList();

  if (nodes.length === 0) {
    const empty = document.createElement("div");
    empty.style.cssText = "color:#9f98b6; font-size:14px; line-height:1.8;";
    empty.textContent = "This note is empty.";
    nodes.push(empty);
  }

  return nodes;
}

function createHeadingNode(text, level) {
  const heading = document.createElement(level <= 2 ? "h2" : "h3");
  heading.style.cssText = `
    margin: ${level <= 2 ? "12px" : "8px"} 0 2px;
    font-size: ${level <= 2 ? "22px" : "18px"};
    font-weight: 680;
    color: #f5f3ff;
    letter-spacing: -0.02em;
  `;
  heading.textContent = text;
  return heading;
}

function createMetaChip(text) {
  const chip = document.createElement("span");
  chip.style.cssText = `
    display:inline-flex;
    align-items:center;
    min-height: 24px;
    padding: 0 9px;
    border-radius: 999px;
    border: 1px solid rgba(255,255,255,0.08);
    background: rgba(255,255,255,0.03);
    color: #cfc7e9;
  `;
  chip.textContent = text;
  return chip;
}

function formatNoteMeta(noteRecord) {
  const sizeText = Number.isFinite(noteRecord?.sizeBytes)
    ? `${noteRecord.sizeBytes} bytes`
    : "size unknown";
  return `${noteRecord?.kind || "note"} · ${sizeText}`;
}
