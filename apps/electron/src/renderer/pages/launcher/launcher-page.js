/**
 * launcher-page.js
 *
 * Vault Launcher: Recent Vaults + Open Vault + Create Vault.
 * Information hierarchy:
 *   1. Recent Vaults
 *   2. Actions (Open Vault primary, Create Vault secondary)
 *   3. Error (only on failure)
 */

import { createRecentVaultsList, addRecentVault } from "./recent-vaults-list.js";

export function createLauncherPage(opts = {}) {
  const { onOpenVault, lastError, isOpening } = opts;

  const page = document.createElement("div");
  page.className = "launcher-page";

  // ── Block 1: Recent Vaults ──
  const recentList = createRecentVaultsList({
    onOpen: (path) => onOpenVault?.(path)
  });
  page.appendChild(recentList);

  // ── Block 2: Actions ──
  const actionsBlock = document.createElement("div");
  actionsBlock.style.cssText = "margin-top: 8px;";

  // Open Vault: primary button, expands input area on click
  const openBtn = document.createElement("button");
  openBtn.textContent = isOpening ? "Opening..." : "Open Vault";
  openBtn.disabled = isOpening;
  openBtn.style.cssText = `
    width: 100%;
    padding: 10px 16px;
    border-radius: 8px;
    border: none;
    background: #111827;
    color: #fff;
    cursor: pointer;
    font-size: 14px;
    font-weight: 500;
    margin-bottom: 8px;
    opacity: ${isOpening ? "0.7" : "1"};
  `;
  actionsBlock.appendChild(openBtn);

  // Path input area (secondary flow, collapsible)
  const inputWrap = document.createElement("div");
  inputWrap.style.cssText = `
    display: flex;
    gap: 8px;
    overflow: hidden;
    max-height: 0;
    opacity: 0;
    transition: max-height 0.25s ease, opacity 0.2s ease, margin 0.2s ease;
    margin-bottom: 0;
  `;

  const pathInput = document.createElement("input");
  pathInput.type = "text";
  pathInput.placeholder = "/path/to/vault";
  pathInput.disabled = isOpening;
  pathInput.style.cssText = `
    flex: 1;
    padding: 8px 12px;
    border-radius: 6px;
    border: 1px solid #d1d5db;
    font-size: 13px;
  `;
  inputWrap.appendChild(pathInput);

  const submitBtn = document.createElement("button");
  submitBtn.textContent = "Open";
  submitBtn.disabled = isOpening;
  submitBtn.style.cssText = `
    padding: 8px 16px;
    border-radius: 6px;
    border: none;
    background: #2563eb;
    color: #fff;
    cursor: pointer;
    font-size: 13px;
    flex-shrink: 0;
  `;
  inputWrap.appendChild(submitBtn);

  actionsBlock.appendChild(inputWrap);

  // Create Vault: secondary button
  const createBtn = document.createElement("button");
  createBtn.textContent = "Create New Vault";
  createBtn.disabled = isOpening;
  createBtn.style.cssText = `
    width: 100%;
    padding: 10px 16px;
    border-radius: 8px;
    border: 1px solid #d1d5db;
    background: #fff;
    color: #374151;
    cursor: pointer;
    font-size: 14px;
    opacity: ${isOpening ? "0.7" : "1"};
  `;
  createBtn.addEventListener("click", () => {
    alert("Create Vault is not yet available.");
  });
  actionsBlock.appendChild(createBtn);

  page.appendChild(actionsBlock);

  // ── Block 3: Error (fixed at bottom, only when present) ──
  const errorArea = document.createElement("div");
  errorArea.style.cssText = "margin-top: 12px; min-height: 0;";

  if (lastError) {
    const errorBlock = document.createElement("div");
    errorBlock.style.cssText = `
      padding: 12px;
      border-radius: 6px;
      background: #fef2f2;
      border: 1px solid #fecaca;
      color: #7f1d1d;
      font-size: 13px;
    `;

    const code = document.createElement("div");
    code.style.fontWeight = "600";
    code.textContent = lastError.code || "Error";
    errorBlock.appendChild(code);

    if (lastError.message) {
      const msg = document.createElement("div");
      msg.style.marginTop = "2px";
      msg.textContent = lastError.message;
      errorBlock.appendChild(msg);
    }

    errorArea.appendChild(errorBlock);
  }

  page.appendChild(errorArea);

  // ── Interactions ──
  let inputExpanded = false;

  function expandInput() {
    inputExpanded = true;
    inputWrap.style.maxHeight = "60px";
    inputWrap.style.opacity = "1";
    inputWrap.style.marginBottom = "8px";
    pathInput.focus();
  }

  function collapseInput() {
    inputExpanded = false;
    inputWrap.style.maxHeight = "0";
    inputWrap.style.opacity = "0";
    inputWrap.style.marginBottom = "0";
  }

  function submit() {
    const path = pathInput.value.trim();
    if (path) {
      addRecentVault(path);
      onOpenVault?.(path);
    }
  }

  openBtn.addEventListener("click", () => {
    if (!inputExpanded) {
      expandInput();
    } else {
      submit();
    }
  });

  submitBtn.addEventListener("click", submit);
  pathInput.addEventListener("keydown", (e) => {
    if (e.key === "Enter") submit();
  });

  // Loading overlay
  if (isOpening) {
    const overlay = document.createElement("div");
    overlay.style.cssText = `
      position: absolute;
      inset: 0;
      display: flex;
      align-items: center;
      justify-content: center;
      background: rgba(255,255,255,0.75);
      border-radius: 14px;
      font-size: 14px;
      color: #4b5563;
      z-index: 1;
    `;
    overlay.textContent = "Opening vault...";
    page.style.position = "relative";
    page.appendChild(overlay);
  }

  return page;
}
