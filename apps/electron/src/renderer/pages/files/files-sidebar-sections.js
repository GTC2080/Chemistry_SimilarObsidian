/**
 * files-sidebar-sections.js
 *
 * Files-mode explorer sections backed by host files surfaces.
 */

export function appendFilesSidebarSections(body, opts = {}) {
  const {
    filesSurfaceState = null,
    currentFilesContentId = null,
    onSelectFilesContent,
    onNavigate,
    tools = []
  } = opts;

  body.appendChild(createSectionLabel("Recent Notes"));
  appendEntryGroup(body, {
    items: filesSurfaceState?.recentEnvelope?.ok ? filesSurfaceState.recentEnvelope.data?.items ?? [] : [],
    emptyMessage: "No recent notes yet.",
    currentFilesContentId,
    onSelectFilesContent,
    loading: Boolean(filesSurfaceState?.loading)
  });

  body.appendChild(createSectionLabel("Vault Root"));
  appendEntryGroup(body, {
    items: filesSurfaceState?.entriesEnvelope?.ok ? filesSurfaceState.entriesEnvelope.data?.items ?? [] : [],
    emptyMessage: "Vault root is empty.",
    currentFilesContentId,
    onSelectFilesContent,
    loading: Boolean(filesSurfaceState?.loading)
  });

  body.appendChild(createSectionLabel("工具"));
  for (const item of tools) {
    body.appendChild(toolShortcutRow(item, () => onNavigate?.(item.id)));
  }
}

function appendEntryGroup(body, opts = {}) {
  const {
    items = [],
    emptyMessage = "No entries.",
    currentFilesContentId = null,
    onSelectFilesContent = null,
    loading = false
  } = opts;

  if (loading) {
    body.appendChild(infoCallout("Loading", "正在从 host Files surface 拉取内容。"));
    return;
  }

  if (!Array.isArray(items) || items.length === 0) {
    body.appendChild(infoCallout("Empty", emptyMessage));
    return;
  }

  for (const item of items) {
    body.appendChild(contentEntryRow({
      icon: pickIcon(item),
      label: item.title || item.name || item.relPath || "Untitled",
      subtitle: item.relPath || item.kind || "entry",
      active: item.relPath === currentFilesContentId,
      onClick: typeof onSelectFilesContent === "function" && item.relPath
        ? () => onSelectFilesContent(item.relPath)
        : null
    }));
  }
}

function createSectionLabel(text) {
  const el = document.createElement("div");
  el.style.cssText = `
    margin: 12px 14px 8px;
    font-size: 11px;
    letter-spacing: 0.18em;
    text-transform: uppercase;
    color: #7f7795;
  `;
  el.textContent = text;
  return el;
}

function contentEntryRow(opts = {}) {
  const { icon = "•", label = "", subtitle = "", active = false, onClick = null } = opts;

  const row = document.createElement(typeof onClick === "function" ? "button" : "div");
  row.style.cssText = `
    display:flex;
    align-items:center;
    gap:12px;
    width: calc(100% - 16px);
    margin: 0 8px 8px;
    padding: 10px 12px;
    border-radius: 14px;
    border: 1px solid ${active ? "rgba(139,92,246,0.28)" : "rgba(255,255,255,0.05)"};
    background: ${active ? "linear-gradient(180deg, rgba(124,58,237,0.16), rgba(124,58,237,0.06))" : "rgba(255,255,255,0.03)"};
    color: #d7d0eb;
    text-align: left;
    cursor: ${typeof onClick === "function" ? "pointer" : "default"};
  `;
  if (typeof onClick === "function") {
    row.addEventListener("mouseenter", () => {
      if (!active) {
        row.style.background = "rgba(255,255,255,0.05)";
      }
    });
    row.addEventListener("mouseleave", () => {
      if (!active) {
        row.style.background = "rgba(255,255,255,0.03)";
      }
    });
    row.addEventListener("click", onClick);
  }

  const iconBox = document.createElement("div");
  iconBox.style.cssText = `
    width: 30px;
    height: 30px;
    border-radius: 10px;
    display:grid;
    place-items:center;
    background: rgba(255,255,255,0.05);
    color: ${active ? "#f5f3ff" : "#bdb5d4"};
    font-size: 14px;
    flex-shrink: 0;
  `;
  iconBox.textContent = icon;
  row.appendChild(iconBox);

  const text = document.createElement("div");
  text.style.cssText = "min-width:0;";
  text.innerHTML = `
    <div style="font-size:13px;font-weight:600;color:${active ? "#f5f3ff" : "#ddd6fe"};">${escapeHtml(label)}</div>
    <div style="font-size:11px;color:#8e87a5;word-break:break-word;">${escapeHtml(subtitle)}</div>
  `;
  row.appendChild(text);
  return row;
}

function toolShortcutRow(item, onClick) {
  const row = document.createElement("button");
  row.style.cssText = `
    display:flex;
    align-items:center;
    justify-content:space-between;
    gap:10px;
    width: calc(100% - 16px);
    margin: 0 8px 8px;
    padding: 10px 12px;
    border-radius: 12px;
    border: 1px solid rgba(255,255,255,0.05);
    background: rgba(255,255,255,0.025);
    color: #cec7e3;
    cursor: pointer;
    text-align: left;
  `;
  row.addEventListener("mouseenter", () => {
    row.style.background = "rgba(255,255,255,0.05)";
  });
  row.addEventListener("mouseleave", () => {
    row.style.background = "rgba(255,255,255,0.025)";
  });
  row.addEventListener("click", onClick);

  const left = document.createElement("div");
  left.style.cssText = "display:flex; align-items:center; gap:10px; min-width:0;";
  left.innerHTML = `
    <span style="width:22px;height:22px;border-radius:8px;display:grid;place-items:center;background:rgba(255,255,255,0.04);font-size:12px;flex-shrink:0;">${escapeHtml(item.icon)}</span>
    <span style="font-size:12px;min-width:0;">${escapeHtml(item.label)}</span>
  `;
  row.appendChild(left);

  const arrow = document.createElement("span");
  arrow.style.cssText = "color:#7f7795; font-size:12px; flex-shrink:0;";
  arrow.textContent = "→";
  row.appendChild(arrow);
  return row;
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

function infoCallout(title, body) {
  const card = document.createElement("div");
  card.style.cssText = `
    margin: 0 8px 12px;
    padding: 12px;
    border-radius: 16px;
    border: 1px solid rgba(139,92,246,0.16);
    background: rgba(124,58,237,0.08);
  `;
  card.innerHTML = `
    <div style="font-size:12px;color:#f5f3ff;font-weight:600;margin-bottom:6px;">${escapeHtml(title)}</div>
    <div style="font-size:12px;color:#cfc7ea;line-height:1.7;">${escapeHtml(body)}</div>
  `;
  return card;
}

function escapeHtml(text) {
  return String(text)
    .replaceAll("&", "&amp;")
    .replaceAll("<", "&lt;")
    .replaceAll(">", "&gt;");
}
