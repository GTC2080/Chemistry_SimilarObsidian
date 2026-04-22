/**
 * app-layout.js
 *
 * Shell chrome: top bar, nav, content area.
 */

import { createNavBar } from "./nav-bar.js";

export function createAppLayout(opts = {}) {
  const { title, currentPage, sessionOpen, activeVaultPath, onNavigate, onCloseVault } = opts;

  const layout = document.createElement("div");
  layout.className = "app-layout";
  layout.style.cssText = `
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
  `;

  const left = document.createElement("div");
  left.style.cssText = "display: flex; align-items: center; gap: 10px;";

  const appTitle = document.createElement("span");
  appTitle.style.cssText = "font-weight: 600; font-size: 15px;";
  appTitle.textContent = title || "Chemistry Obsidian";
  left.appendChild(appTitle);

  if (activeVaultPath) {
    const vaultLabel = document.createElement("span");
    vaultLabel.style.cssText = "font-size: 12px; color: #6b7280;";
    vaultLabel.textContent = activeVaultPath;
    left.appendChild(vaultLabel);
  }

  topBar.appendChild(left);

  const right = document.createElement("div");
  right.style.cssText = "display: flex; align-items: center; gap: 10px;";

  // Runtime status badge container
  const badgeContainer = document.createElement("div");
  badgeContainer.id = "runtime-status-badge-container";
  right.appendChild(badgeContainer);

  if (sessionOpen && typeof onCloseVault === "function") {
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
  layout.appendChild(topBar);

  // Nav bar
  const nav = createNavBar(currentPage, { onNavigate, sessionOpen });
  layout.appendChild(nav);

  // Content area
  const content = document.createElement("main");
  content.id = "app-content";
  content.style.cssText = `
    flex: 1;
    overflow: auto;
    padding: 16px;
    background: #f3f4f6;
  `;
  layout.appendChild(content);

  return { element: layout, contentArea: content };
}
