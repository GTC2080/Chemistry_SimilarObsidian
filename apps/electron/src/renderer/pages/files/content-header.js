/**
 * content-header.js
 *
 * Lightweight header for the Files content surface.
 */

export function createFilesContentHeader(opts = {}) {
  const {
    title = "Workspace Home",
    subtitle = "",
    runtimeLine = "session=unknown · index=unknown"
  } = opts;

  const header = document.createElement("header");
  header.style.cssText = `
    display: flex;
    flex-direction: column;
    gap: 10px;
    padding: 20px 22px 18px;
    border-bottom: 1px solid rgba(255,255,255,0.06);
    background:
      radial-gradient(circle at top right, rgba(139,92,246,0.12), transparent 32%),
      linear-gradient(180deg, rgba(33,31,39,0.98), rgba(24,23,29,0.98));
  `;

  const metaRow = document.createElement("div");
  metaRow.style.cssText = "display:flex; align-items:center; gap:8px; flex-wrap:wrap;";
  metaRow.appendChild(createChip("Files"));
  metaRow.appendChild(createChip("Content Surface"));

  const subtitleChip = createChip(subtitle || "Current Vault");
  subtitleChip.style.background = "rgba(255,255,255,0.04)";
  subtitleChip.style.color = "#d6cff6";
  metaRow.appendChild(subtitleChip);
  header.appendChild(metaRow);

  const titleEl = document.createElement("div");
  titleEl.style.cssText = "font-size: 28px; font-weight: 700; letter-spacing: -0.03em; color: #faf7ff;";
  titleEl.textContent = title;
  header.appendChild(titleEl);

  const runtimeMeta = document.createElement("div");
  runtimeMeta.style.cssText = "font-size: 12px; color: #938ca9;";
  runtimeMeta.textContent = runtimeLine;
  header.appendChild(runtimeMeta);

  return header;
}

function createChip(text) {
  const chip = document.createElement("span");
  chip.style.cssText = `
    display: inline-flex;
    align-items: center;
    min-height: 28px;
    padding: 0 11px;
    border-radius: 999px;
    border: 1px solid rgba(255,255,255,0.08);
    background: rgba(124,58,237,0.12);
    color: #c4b5fd;
    font-size: 11px;
    letter-spacing: 0.14em;
    text-transform: uppercase;
  `;
  chip.textContent = text;
  return chip;
}
