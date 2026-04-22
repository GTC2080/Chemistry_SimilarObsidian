/**
 * launcher-page.js
 *
 * Vault Launcher: Recent Vaults + Open Vault + Create Vault.
 * This is the only screen visible when no vault is open.
 */

import { createRecentVaultsList, addRecentVault } from "./recent-vaults-list.js";

export function createLauncherPage(opts = {}) {
  const { onOpenVault, lastError, isOpening } = opts;

  const page = document.createElement("div");
  page.className = "launcher-page";

  // Recent Vaults
  const recentList = createRecentVaultsList({
    onOpen: (path) => onOpenVault?.(path)
  });
  page.appendChild(recentList);

  // Divider
  const divider = document.createElement("div");
  divider.style.cssText = "height: 1px; background: #e5e7eb; margin: 16px 0;";
  page.appendChild(divider);

  // Open Vault section
  const openSection = document.createElement("div");
  openSection.style.cssText = "margin-bottom: 12px;";

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
    margin-bottom: 10px;
    opacity: ${isOpening ? "0.7" : "1"};
  `;
  openSection.appendChild(openBtn);

  const inputWrap = document.createElement("div");
  inputWrap.style.cssText = `
    display: flex;
    gap: 8px;
    overflow: hidden;
    transition: max-height 0.2s ease;
    max-height: 0;
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
  `;
  inputWrap.appendChild(submitBtn);

  openSection.appendChild(inputWrap);
  page.appendChild(openSection);

  // Create Vault placeholder
  const createBtn = document.createElement("button");
  createBtn.textContent = "Create New Vault";
  createBtn.style.cssText = `
    width: 100%;
    padding: 10px 16px;
    border-radius: 8px;
    border: 1px solid #d1d5db;
    background: #fff;
    color: #374151;
    cursor: pointer;
    font-size: 14px;
    margin-bottom: 12px;
  `;
  createBtn.addEventListener("click", () => {
    alert("Create Vault is not yet available.");
  });
  page.appendChild(createBtn);

  // Error area (fixed at bottom of card)
  const errorArea = document.createElement("div");
  errorArea.style.cssText = "min-height: 0;";
  page.appendChild(errorArea);

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
      msg.textContent = lastError.message;
      errorBlock.appendChild(msg);
    }

    errorArea.appendChild(errorBlock);
  }

  // Interactions
  let inputExpanded = false;

  function expandInput() {
    inputExpanded = true;
    inputWrap.style.maxHeight = "60px";
    pathInput.focus();
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
      background: rgba(255,255,255,0.7);
      border-radius: 14px;
      font-size: 14px;
      color: #4b5563;
    `;
    overlay.textContent = "Opening vault...";
    page.style.position = "relative";
    page.appendChild(overlay);
  }

  return page;
}
