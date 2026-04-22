/**
 * workspace-shell.js
 *
 * Obsidian-inspired workspace layout: top tab strip + left explorer + center
 * editor surface + right host summary rail.
 */

import { createSidebar } from "./sidebar.js";
import { createRuntimeStatusBadge } from "../shared/runtime-status-badge.js";

const PAGE_META = {
  files: { label: "文件", subtitle: "Vault Home" },
  search: { label: "搜索", subtitle: "Search & Retrieval" },
  attachments: { label: "附件", subtitle: "Attachment Surface" },
  chemistry: { label: "化学", subtitle: "Chemistry Spectra" },
  diagnostics: { label: "诊断", subtitle: "Diagnostics & Rebuild" }
};

export function createWorkspaceShell(opts = {}) {
  const {
    currentPage,
    vaultName,
    runtimeEnvelope,
    filesSurfaceState,
    onNavigate,
    onCloseVault,
    currentFilesContentId,
    onSelectFilesContent,
    children
  } = opts;
  const currentMeta = PAGE_META[currentPage] ?? PAGE_META.files;

  const shell = document.createElement("div");
  shell.className = "workspace-shell";
  shell.style.cssText = `
    display: flex;
    flex-direction: column;
    height: 100vh;
    overflow: hidden;
    font-family: "Segoe UI", "PingFang SC", sans-serif;
    color: #f5f3ff;
    background:
      radial-gradient(circle at top right, rgba(124, 58, 237, 0.16), transparent 24%),
      linear-gradient(180deg, #17161c 0%, #111015 100%);
  `;

  shell.appendChild(createTopChrome(currentMeta, vaultName, runtimeEnvelope, onCloseVault));

  const body = document.createElement("div");
  body.style.cssText = "flex: 1; display: flex; min-height: 0; overflow: hidden;";

  const sidebar = createSidebar(currentPage, {
    onNavigate,
    onCloseVault,
    vaultPath: vaultName,
    filesSurfaceState,
    currentFilesContentId,
    onSelectFilesContent
  });
  body.appendChild(sidebar);

  const center = document.createElement("section");
  center.style.cssText = `
    flex: 1;
    min-width: 0;
    display: flex;
    flex-direction: column;
    background: rgba(15, 14, 20, 0.4);
  `;

  center.appendChild(createEditorHeader(currentPage, currentMeta, vaultName));

  const content = document.createElement("main");
  content.className = "workspace-content";
  content.style.cssText = `
    flex: 1;
    min-height: 0;
    overflow: auto;
    padding: 18px 22px 22px;
  `;

  if (children) {
    content.appendChild(children);
  }

  center.appendChild(content);
  body.appendChild(center);
  body.appendChild(createInspectorRail(runtimeEnvelope, currentMeta, vaultName));
  shell.appendChild(body);

  return { element: shell, contentArea: content };
}

function createTopChrome(currentMeta, vaultName, runtimeEnvelope, onCloseVault) {
  const topBar = document.createElement("header");
  topBar.style.cssText = `
    display: flex;
    align-items: center;
    justify-content: space-between;
    gap: 18px;
    min-height: 50px;
    padding: 0 18px;
    border-bottom: 1px solid rgba(255,255,255,0.08);
    background: rgba(29, 27, 34, 0.94);
    flex-shrink: 0;
  `;

  const left = document.createElement("div");
  left.style.cssText = "display:flex; align-items:center; gap:12px; min-width:0;";

  const brandDot = document.createElement("div");
  brandDot.style.cssText = `
    width: 12px;
    height: 12px;
    border-radius: 999px;
    background: conic-gradient(from 90deg, #8b5cf6, #c084fc, #8b5cf6);
    box-shadow: 0 0 18px rgba(139,92,246,0.34);
    flex-shrink: 0;
  `;
  left.appendChild(brandDot);

  const brandText = document.createElement("div");
  brandText.style.cssText = "min-width:0;";
  brandText.innerHTML = `
    <div style="font-size:13px;font-weight:600;color:#f5f3ff;">${escapeHtml(baseName(vaultName) || "workspace")}</div>
    <div style="font-size:11px;color:#867f9d;">Chemistry Obsidian · ${escapeHtml(currentMeta.label)}</div>
  `;
  left.appendChild(brandText);

  topBar.appendChild(left);

  const right = document.createElement("div");
  right.style.cssText = "display:flex; align-items:center; gap:10px;";
  right.appendChild(createRuntimeStatusBadge(runtimeEnvelope));
  if (typeof onCloseVault === "function") {
    const closeBtn = document.createElement("button");
    closeBtn.textContent = "Close Vault";
    closeBtn.style.cssText = `
      padding: 8px 14px;
      border-radius: 10px;
      border: 1px solid rgba(255,255,255,0.08);
      background: rgba(255,255,255,0.04);
      color: #f3ecff;
      cursor: pointer;
      font-size: 12px;
    `;
    closeBtn.addEventListener("click", onCloseVault);
    right.appendChild(closeBtn);
  }
  topBar.appendChild(right);

  return topBar;
}

function createEditorHeader(currentPage, currentMeta, vaultName) {
  const header = document.createElement("div");
  header.style.cssText = `
    display:flex;
    align-items:center;
    justify-content:space-between;
    gap:16px;
    min-height: 54px;
    padding: 0 22px;
    border-bottom: 1px solid rgba(255,255,255,0.06);
    background: rgba(18,17,22,0.42);
    flex-shrink: 0;
  `;

  const left = document.createElement("div");
  left.style.cssText = "display:flex; align-items:center; gap:12px; min-width:0;";

  const titleWrap = document.createElement("div");
  titleWrap.style.cssText = "min-width:0;";
  titleWrap.innerHTML = `
    <div style="font-size:18px;font-weight:600;color:#f5f3ff;">${escapeHtml(currentMeta.label)}</div>
    <div style="font-size:12px;color:#8d86a4;">${escapeHtml(currentMeta.subtitle)} · ${escapeHtml(baseName(vaultName) || "workspace")}</div>
  `;
  left.appendChild(titleWrap);
  header.appendChild(left);

  const right = document.createElement("div");
  right.style.cssText = "display:flex; align-items:center; gap:8px;";
  const hint = document.createElement("div");
  hint.style.cssText = `
    padding: 7px 10px;
    border-radius: 999px;
    border: 1px solid rgba(255,255,255,0.06);
    background: rgba(255,255,255,0.03);
    color: #8e87a5;
    font-size: 11px;
    letter-spacing: 0.12em;
    text-transform: uppercase;
  `;
  hint.textContent = currentPage === "diagnostics" ? "Tools" : "Content";
  right.appendChild(hint);
  header.appendChild(right);

  return header;
}

function createInspectorRail(runtimeEnvelope, currentMeta, vaultName) {
  const rail = document.createElement("aside");
  rail.style.cssText = `
    width: 208px;
    min-width: 208px;
    border-left: 1px solid rgba(255,255,255,0.08);
    background: linear-gradient(180deg, rgba(27,25,32,0.96) 0%, rgba(20,18,24,0.96) 100%);
    display:flex;
    flex-direction:column;
    overflow:hidden;
  `;

  const header = document.createElement("div");
  header.style.cssText = `
    display:flex;
    align-items:center;
    gap:10px;
    padding: 16px 16px 12px;
    border-bottom: 1px solid rgba(255,255,255,0.06);
  `;
  header.innerHTML = `
    <div style="width:14px;height:14px;border-radius:999px;background:conic-gradient(from 90deg, #fb923c, #f472b6, #8b5cf6, #fb923c); box-shadow:0 0 18px rgba(139,92,246,0.4);"></div>
    <div style="font-size:13px;font-weight:700;color:#d8d1ec;">Context</div>
  `;
  rail.appendChild(header);

  const body = document.createElement("div");
  body.style.cssText = "flex:1; overflow:auto; padding: 16px;";

  body.appendChild(infoCard("Surface", currentMeta.label, currentMeta.subtitle));
  body.appendChild(infoCard("Vault", baseName(vaultName) || "workspace", vaultName || "No active path"));
  body.appendChild(runtimeSummaryCard(runtimeEnvelope));

  rail.appendChild(body);
  return rail;
}

function runtimeSummaryCard(runtimeEnvelope) {
  const card = document.createElement("div");
  card.style.cssText = `
    margin-top: 14px;
    padding: 12px;
    border-radius: 16px;
    border: 1px solid rgba(255,255,255,0.06);
    background: rgba(255,255,255,0.03);
  `;
  const title = document.createElement("div");
  title.style.cssText = "font-size:11px;letter-spacing:0.16em;text-transform:uppercase;color:#8b83a2;margin-bottom:8px;";
  title.textContent = "Runtime";
  card.appendChild(title);

  const runtime = runtimeEnvelope?.ok ? runtimeEnvelope.data : null;
  const rows = [
    { label: "Lifecycle", value: runtime?.lifecycle_state ?? "unknown" },
    { label: "Index", value: runtime?.kernel_runtime?.index_state ?? "unknown" },
    { label: "Binding", value: runtime?.kernel_binding?.attached ? "attached" : "detached" },
    { label: "Rebuild", value: runtime?.rebuild?.in_flight ? "in flight" : "idle" }
  ];
  for (const row of rows) {
    const line = document.createElement("div");
    line.style.cssText = "display:flex;justify-content:space-between;gap:8px;padding:6px 0;font-size:12px;";
    line.innerHTML = `
      <span style="color:#9a93b0;">${escapeHtml(row.label)}</span>
      <span style="color:#f5f3ff;">${escapeHtml(row.value)}</span>
    `;
    card.appendChild(line);
  }
  return card;
}

function infoCard(label, title, body) {
  const card = document.createElement("div");
  card.style.cssText = `
    margin-top: 12px;
    padding: 12px;
    border-radius: 16px;
    border: 1px solid rgba(255,255,255,0.06);
    background: rgba(255,255,255,0.03);
  `;
  card.innerHTML = `
    <div style="font-size:11px;letter-spacing:0.16em;text-transform:uppercase;color:#8b83a2;margin-bottom:8px;">${escapeHtml(label)}</div>
    <div style="font-size:14px;font-weight:600;color:#f5f3ff;">${escapeHtml(title)}</div>
    <div style="margin-top:6px;font-size:11px;color:#978fae;line-height:1.6;word-break:break-word;">${escapeHtml(body)}</div>
  `;
  return card;
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
