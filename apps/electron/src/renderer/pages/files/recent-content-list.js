/**
 * recent-content-list.js
 *
 * Host-backed recent notes and vault-root listing for Files mode.
 */

export function createRecentContentList(opts = {}) {
  const {
    entriesEnvelope,
    recentEnvelope,
    selectedRelPath = "",
    loading = false,
    loadingSelection = false,
    onSelect = null
  } = opts;

  const wrapper = document.createElement("section");
  wrapper.appendChild(createSectionLabel("Recent Notes"));

  if (loading) {
    wrapper.appendChild(createNoticeCard("Loading", "正在从当前 vault 拉取 recent notes 和 Files 根目录内容。"));
    return wrapper;
  }

  wrapper.appendChild(createEnvelopeSection({
    items: recentEnvelope?.ok ? recentEnvelope.data?.items ?? [] : [],
    selectedRelPath,
    onSelect,
    emptyMessage: "当前 vault 还没有 recent note 记录。",
    errorEnvelope: recentEnvelope,
    loadingSelection
  }));

  wrapper.appendChild(createSectionLabel("Vault Root"));
  wrapper.appendChild(createEnvelopeSection({
    items: entriesEnvelope?.ok ? entriesEnvelope.data?.items ?? [] : [],
    selectedRelPath,
    onSelect,
    emptyMessage: "当前 vault 根目录没有可显示内容。",
    errorEnvelope: entriesEnvelope,
    loadingSelection
  }));

  return wrapper;
}

function createEnvelopeSection(opts = {}) {
  const {
    items = [],
    selectedRelPath = "",
    onSelect = null,
    emptyMessage = "No items.",
    errorEnvelope,
    loadingSelection = false
  } = opts;

  if (errorEnvelope && errorEnvelope.ok === false) {
    return createNoticeCard(
      errorEnvelope.error?.code || "Files unavailable",
      errorEnvelope.error?.message || "Files surface is unavailable."
    );
  }

  if (!Array.isArray(items) || items.length === 0) {
    return createNoticeCard("Empty", emptyMessage);
  }

  const list = document.createElement("div");
  list.style.cssText = "display:grid; gap:10px;";
  for (const item of items) {
    list.appendChild(createItem(item, item.relPath === selectedRelPath, onSelect, loadingSelection));
  }
  return list;
}

function createSectionLabel(text) {
  const el = document.createElement("div");
  el.style.cssText = `
    margin: 0 0 12px;
    font-size: 11px;
    letter-spacing: 0.2em;
    text-transform: uppercase;
    color: #938ca9;
  `;
  el.textContent = text;
  return el;
}

function createItem(item, active, onSelect, loadingSelection) {
  const relPath = item?.relPath ?? "";
  const clickable = typeof onSelect === "function" && relPath;
  const card = document.createElement(clickable ? "button" : "div");
  card.style.cssText = `
    display:flex;
    align-items:flex-start;
    gap:12px;
    width: 100%;
    padding: 14px;
    border-radius: 18px;
    border: 1px solid ${active ? "rgba(139,92,246,0.28)" : "rgba(255,255,255,0.05)"};
    background: ${active ? "linear-gradient(180deg, rgba(124,58,237,0.16), rgba(124,58,237,0.06))" : "rgba(255,255,255,0.03)"};
    text-align: left;
    cursor: ${clickable ? "pointer" : "default"};
    opacity: ${loadingSelection && active ? "0.8" : "1"};
  `;
  if (clickable) {
    card.addEventListener("mouseenter", () => {
      if (!active) {
        card.style.background = "rgba(255,255,255,0.05)";
      }
    });
    card.addEventListener("mouseleave", () => {
      if (!active) {
        card.style.background = "rgba(255,255,255,0.03)";
      }
    });
    card.addEventListener("click", () => onSelect(relPath));
  }

  const icon = document.createElement("div");
  icon.style.cssText = `
    width: 36px;
    height: 36px;
    border-radius: 12px;
    display:grid;
    place-items:center;
    background: rgba(255,255,255,0.04);
    color: #c9c0ef;
    font-size: 15px;
    flex-shrink: 0;
  `;
  icon.textContent = pickIcon(item);
  card.appendChild(icon);

  const body = document.createElement("div");
  body.style.cssText = "min-width:0; flex:1;";

  const top = document.createElement("div");
  top.style.cssText = "display:flex; align-items:center; justify-content:space-between; gap:8px;";

  const title = document.createElement("div");
  title.style.cssText = "font-size:14px; font-weight:600; color:#f5f3ff; min-width:0; overflow:hidden; text-overflow:ellipsis; white-space:nowrap;";
  title.textContent = item.title || item.name || relPath || "Untitled";
  top.appendChild(title);

  const badge = document.createElement("span");
  badge.style.cssText = `
    display:inline-flex;
    align-items:center;
    min-height: 22px;
    padding: 0 8px;
    border-radius: 999px;
    background: rgba(255,255,255,0.05);
    color: #b7afd0;
    font-size: 10px;
    letter-spacing: 0.12em;
    text-transform: uppercase;
    flex-shrink: 0;
  `;
  badge.textContent = active && loadingSelection ? "Loading" : formatKind(item);
  top.appendChild(badge);
  body.appendChild(top);

  const subtitle = document.createElement("div");
  subtitle.style.cssText = "margin-top:6px; font-size:12px; line-height:1.6; color:#968ead; word-break:break-word;";
  subtitle.textContent = relPath || item.name || "No path";
  body.appendChild(subtitle);

  card.appendChild(body);
  return card;
}

function createNoticeCard(title, body) {
  const card = document.createElement("div");
  card.style.cssText = `
    margin-bottom: 14px;
    padding: 12px;
    border-radius: 16px;
    border: 1px solid rgba(255,255,255,0.06);
    background: rgba(255,255,255,0.03);
  `;
  card.innerHTML = `
    <div style="font-size:11px; letter-spacing:0.16em; text-transform:uppercase; color:#c4b5fd; margin-bottom:8px;">${escapeHtml(title)}</div>
    <div style="font-size:12px; line-height:1.7; color:#ddd6fe;">${escapeHtml(body)}</div>
  `;
  return card;
}

function pickIcon(item) {
  if (item?.isDirectory || item?.kind === "directory") {
    return "▣";
  }

  if (item?.kind === "note") {
    return "✦";
  }

  return "◫";
}

function formatKind(item) {
  if (item?.isDirectory || item?.kind === "directory") {
    return "Folder";
  }

  if (item?.kind === "note") {
    return "Note";
  }

  if (item?.kind === "attachment") {
    return "Asset";
  }

  return item?.kind || "Entry";
}

function escapeHtml(text) {
  return String(text)
    .replaceAll("&", "&amp;")
    .replaceAll("<", "&lt;")
    .replaceAll(">", "&gt;");
}
