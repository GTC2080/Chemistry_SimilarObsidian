/**
 * sidebar.js
 *
 * Obsidian-like left stack: activity rail + explorer pane.
 */

const ITEMS = [
  { id: "files", label: "文件", subtitle: "Vault Home", icon: "⌂" },
  { id: "search", label: "搜索", subtitle: "Query surfaces", icon: "⌕" },
  { id: "attachments", label: "附件", subtitle: "Path & refs", icon: "⊡" },
  { id: "chemistry", label: "化学", subtitle: "Spectra substrate", icon: "⚗" },
  { id: "diagnostics", label: "诊断", subtitle: "Support bundle", icon: "⌁" }
];

export function createSidebar(currentPage, opts = {}) {
  const { onNavigate, onCloseVault, vaultPath } = opts;

  const sidebar = document.createElement("aside");
  sidebar.className = "workspace-sidebar";
  sidebar.style.cssText = `
    width: 344px;
    display: grid;
    grid-template-columns: 58px minmax(0, 1fr);
    border-right: 1px solid rgba(255,255,255,0.08);
    background: linear-gradient(180deg, #232127 0%, #1e1c22 100%);
    flex-shrink: 0;
    overflow: hidden;
  `;

  sidebar.appendChild(createActivityRail(currentPage, { onNavigate }));
  sidebar.appendChild(createExplorerPane(currentPage, { onNavigate, onCloseVault, vaultPath }));
  return sidebar;
}

function createActivityRail(currentPage, opts = {}) {
  const { onNavigate } = opts;
  const rail = document.createElement("div");
  rail.style.cssText = `
    display: flex;
    flex-direction: column;
    align-items: center;
    gap: 10px;
    padding: 14px 10px;
    background: rgba(18, 17, 22, 0.74);
    border-right: 1px solid rgba(255,255,255,0.06);
  `;

  for (const item of ITEMS) {
    const btn = document.createElement("button");
    const active = item.id === currentPage;
    btn.textContent = item.icon;
    btn.title = item.label;
    btn.style.cssText = `
      width: 38px;
      height: 38px;
      border-radius: 12px;
      border: 1px solid ${active ? "rgba(139,92,246,0.48)" : "transparent"};
      background: ${active ? "linear-gradient(180deg, rgba(131, 93, 255, 0.28), rgba(107, 71, 225, 0.16))" : "transparent"};
      color: ${active ? "#f3e8ff" : "#b9b1cf"};
      font-size: 18px;
      cursor: pointer;
      transition: 120ms ease;
    `;
    btn.addEventListener("mouseenter", () => {
      if (!active) {
        btn.style.background = "rgba(255,255,255,0.06)";
        btn.style.color = "#f5f3ff";
      }
    });
    btn.addEventListener("mouseleave", () => {
      if (!active) {
        btn.style.background = "transparent";
        btn.style.color = "#b9b1cf";
      }
    });
    btn.addEventListener("click", () => onNavigate?.(item.id));
    rail.appendChild(btn);
  }

  const spacer = document.createElement("div");
  spacer.style.flex = "1";
  rail.appendChild(spacer);

  const buildMark = document.createElement("div");
  buildMark.textContent = "01";
  buildMark.style.cssText = `
    font-size: 11px;
    letter-spacing: 0.2em;
    color: #7d7690;
    writing-mode: vertical-rl;
    text-orientation: mixed;
    opacity: 0.8;
  `;
  rail.appendChild(buildMark);

  return rail;
}

function createExplorerPane(currentPage, opts = {}) {
  const { onNavigate, onCloseVault, vaultPath } = opts;

  const pane = document.createElement("div");
  pane.style.cssText = `
    min-width: 0;
    display: flex;
    flex-direction: column;
    overflow: hidden;
  `;

  const header = document.createElement("div");
  header.style.cssText = `
    display: flex;
    align-items: center;
    justify-content: space-between;
    padding: 16px 18px 12px;
    border-bottom: 1px solid rgba(255,255,255,0.06);
  `;

  const title = document.createElement("div");
  title.innerHTML = `
    <div style="font-size:11px;letter-spacing:0.22em;color:#8c84a3;text-transform:uppercase;">Explorer</div>
    <div style="margin-top:6px;font-size:14px;font-weight:600;color:#f5f3ff;">${escapeHtml(baseName(vaultPath) || "Current Vault")}</div>
  `;
  header.appendChild(title);

  const controls = document.createElement("div");
  controls.style.cssText = "display:flex;gap:6px;";
  for (const label of ["＋", "⋯"]) {
    const btn = document.createElement("button");
    btn.textContent = label;
    btn.style.cssText = `
      width: 30px;
      height: 30px;
      border-radius: 9px;
      border: 1px solid rgba(255,255,255,0.08);
      background: rgba(255,255,255,0.04);
      color: #d4cffa;
      cursor: pointer;
      font-size: 15px;
    `;
    controls.appendChild(btn);
  }
  header.appendChild(controls);
  pane.appendChild(header);

  const body = document.createElement("div");
  body.style.cssText = `
    flex: 1;
    overflow: auto;
    padding: 14px 10px 16px;
  `;

  body.appendChild(sectionLabel("导航"));
  for (const item of ITEMS) {
    body.appendChild(explorerRow(item, item.id === currentPage, () => onNavigate?.(item.id)));
  }

  body.appendChild(sectionLabel("当前工作区"));
  const metaCard = document.createElement("div");
  metaCard.style.cssText = `
    margin: 0 8px 12px;
    padding: 12px;
    border-radius: 16px;
    border: 1px solid rgba(255,255,255,0.06);
    background: rgba(255,255,255,0.03);
  `;
  metaCard.innerHTML = `
    <div style="font-size:12px;color:#f5f3ff;font-weight:600;margin-bottom:6px;">${escapeHtml(baseName(vaultPath) || "vault")}</div>
    <div style="font-size:12px;color:#8f88a7;line-height:1.6;word-break:break-all;">${escapeHtml(vaultPath || "No active vault path")}</div>
  `;
  body.appendChild(metaCard);

  const tree = document.createElement("div");
  tree.style.cssText = "margin: 0 8px; display:flex; flex-direction:column; gap:4px;";
  const treeRows = [
    { depth: 0, icon: "▾", label: "host surfaces" },
    { depth: 1, icon: "•", label: "files" },
    { depth: 1, icon: "•", label: "search" },
    { depth: 1, icon: "•", label: "attachments" },
    { depth: 1, icon: "•", label: "chemistry" },
    { depth: 1, icon: "•", label: "diagnostics" }
  ];
  for (const row of treeRows) {
    const line = document.createElement("div");
    line.style.cssText = `
      display:flex;
      align-items:center;
      gap:8px;
      min-height:28px;
      padding: 0 10px 0 ${10 + row.depth * 18}px;
      border-radius: 10px;
      color: #a59dbd;
      font-size: 13px;
    `;
    line.innerHTML = `<span style="width:12px;color:#756e88;">${row.icon}</span><span>${escapeHtml(row.label)}</span>`;
    tree.appendChild(line);
  }
  body.appendChild(tree);
  pane.appendChild(body);

  const footer = document.createElement("div");
  footer.style.cssText = `
    display:flex;
    align-items:center;
    justify-content:space-between;
    gap:10px;
    padding: 14px 16px;
    border-top: 1px solid rgba(255,255,255,0.06);
    background: rgba(0,0,0,0.14);
  `;

  const footerMeta = document.createElement("div");
  footerMeta.innerHTML = `
    <div style="font-size:12px;color:#f5f3ff;">${escapeHtml(baseName(vaultPath) || "workspace")}</div>
    <div style="font-size:11px;color:#827a97;">sealed host workspace</div>
  `;
  footer.appendChild(footerMeta);

  if (typeof onCloseVault === "function") {
    const closeBtn = document.createElement("button");
    closeBtn.textContent = "关闭";
    closeBtn.style.cssText = `
      padding: 8px 14px;
      border-radius: 10px;
      border: 1px solid rgba(255,255,255,0.08);
      background: rgba(255,255,255,0.05);
      color: #f3ecff;
      cursor: pointer;
      font-size: 12px;
    `;
    closeBtn.addEventListener("click", onCloseVault);
    footer.appendChild(closeBtn);
  }

  pane.appendChild(footer);

  return pane;
}

function sectionLabel(text) {
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

function explorerRow(item, active, onClick) {
  const row = document.createElement("button");
  row.style.cssText = `
    display:flex;
    align-items:center;
    gap:12px;
    width: calc(100% - 16px);
    margin: 0 8px;
    padding: 10px 12px;
    border-radius: 14px;
    border: 1px solid ${active ? "rgba(139,92,246,0.3)" : "transparent"};
    background: ${active ? "linear-gradient(180deg, rgba(124,58,237,0.16), rgba(124,58,237,0.06))" : "transparent"};
    color: ${active ? "#f5f3ff" : "#c6bfd9"};
    cursor: pointer;
    text-align: left;
  `;
  row.addEventListener("mouseenter", () => {
    if (!active) row.style.background = "rgba(255,255,255,0.045)";
  });
  row.addEventListener("mouseleave", () => {
    if (!active) row.style.background = "transparent";
  });
  row.addEventListener("click", onClick);

  const icon = document.createElement("div");
  icon.textContent = item.icon;
  icon.style.cssText = `
    width: 30px;
    height: 30px;
    border-radius: 10px;
    display:grid;
    place-items:center;
    background: ${active ? "rgba(255,255,255,0.08)" : "rgba(255,255,255,0.04)"};
    color: inherit;
    font-size: 15px;
    flex-shrink: 0;
  `;
  row.appendChild(icon);

  const text = document.createElement("div");
  text.style.cssText = "min-width:0;";
  text.innerHTML = `
    <div style="font-size:13px;font-weight:600;">${escapeHtml(item.label)}</div>
    <div style="font-size:11px;color:${active ? "#bfb5ff" : "#8e87a5"};">${escapeHtml(item.subtitle)}</div>
  `;
  row.appendChild(text);

  return row;
}

function baseName(vaultPath) {
  if (!vaultPath || typeof vaultPath !== "string") return "";
  const parts = vaultPath.split(/[\\/]/).filter(Boolean);
  return parts.length > 0 ? parts[parts.length - 1] : vaultPath;
}

function escapeHtml(text) {
  return String(text)
    .replaceAll("&", "&amp;")
    .replaceAll("<", "&lt;")
    .replaceAll(">", "&gt;");
}
