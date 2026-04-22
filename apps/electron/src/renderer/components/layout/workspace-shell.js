/**
 * workspace-shell.js
 *
 * Workspace layout: top bar + sidebar + content area.
 */

import { createSidebar } from "./sidebar.js";
import { createRuntimeStatusBadge } from "../shared/runtime-status-badge.js";

export function createWorkspaceShell(opts = {}) {
  const { currentPage, vaultName, runtimeEnvelope, onNavigate, onCloseVault, children } = opts;

  const shell = document.createElement("div");
  shell.className = "workspace-shell";
  shell.style.cssText = `
    display: flex;
    flex-direction: column;
    height: 100vh;
    overflow: hidden;
    font-family: "Segoe UI", "PingFang SC", sans-serif;
    color: #1f2937;
  `;

  // Top bar
  const topBar = document.createElement("header");
  topBar.style.cssText = `
    display: flex;
    align-items: center;
    justify-content: space-between;
    padding: 10px 16px;
    border-bottom: 1px solid #e5e7eb;
    background: #fff;
    flex-shrink: 0;
  `;

  const left = document.createElement("div");
  left.style.cssText = "display: flex; align-items: center; gap: 10px;";

  const appTitle = document.createElement("span");
  appTitle.style.cssText = "font-size: 13px; color: #9ca3af;";
  appTitle.textContent = "Chemistry Obsidian";
  left.appendChild(appTitle);

  const vaultLabel = document.createElement("span");
  vaultLabel.style.cssText = "font-weight: 600; font-size: 15px; color: #111827;";
  vaultLabel.textContent = vaultName || "Vault";
  left.appendChild(vaultLabel);

  topBar.appendChild(left);

  const right = document.createElement("div");
  right.style.cssText = "display: flex; gap: 10px; align-items: center;";

  right.appendChild(createRuntimeStatusBadge(runtimeEnvelope));

  if (typeof onCloseVault === "function") {
    const closeBtn = document.createElement("button");
    closeBtn.textContent = "Close Vault";
    closeBtn.style.cssText = `
      padding: 5px 12px;
      border-radius: 6px;
      border: 1px solid #d1d5db;
      background: #fff;
      cursor: pointer;
      font-size: 12px;
    `;
    closeBtn.addEventListener("click", onCloseVault);
    right.appendChild(closeBtn);
  }

  topBar.appendChild(right);
  shell.appendChild(topBar);

  // Body: sidebar + content
  const body = document.createElement("div");
  body.style.cssText = "flex: 1; display: flex; overflow: hidden;";

  const sidebar = createSidebar(currentPage, { onNavigate });
  body.appendChild(sidebar);

  const content = document.createElement("main");
  content.className = "workspace-content";
  content.style.cssText = `
    flex: 1;
    padding: 16px;
    overflow: auto;
    background: #f3f4f6;
  `;

  if (children) {
    content.appendChild(children);
  }

  body.appendChild(content);
  shell.appendChild(body);

  return { element: shell, contentArea: content };
}
