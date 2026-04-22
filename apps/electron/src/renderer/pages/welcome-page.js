/**
 * welcome-page.js
 *
 * Vault not opened; session entry page.
 */

import { createStateSurface } from "../components/shared/state-surface.js";

export function createWelcomePage(opts = {}) {
  const { onOpenVault, lastError, sessionState } = opts;
  const isBusy = sessionState === "opening" || sessionState === "closing";

  const page = document.createElement("div");
  page.className = "welcome-page";
  page.style.cssText = `
    max-width: 480px;
    margin: 60px auto;
    padding: 32px;
    border-radius: 12px;
    background: #fff;
    border: 1px solid #e5e7eb;
    box-shadow: 0 4px 12px rgba(0,0,0,0.04);
  `;

  const heading = document.createElement("h2");
  heading.style.cssText = "margin: 0 0 8px; font-size: 20px;";
  heading.textContent = "Open a Vault";
  page.appendChild(heading);

  const sub = document.createElement("p");
  sub.style.cssText = "margin: 0 0 20px; color: #6b7280; font-size: 13px;";
  sub.textContent = "Enter the vault path and click Open Vault.";
  page.appendChild(sub);

  const inputWrap = document.createElement("div");
  inputWrap.style.cssText = "display: flex; gap: 8px; margin-bottom: 16px;";

  const input = document.createElement("input");
  input.type = "text";
  input.placeholder = "/path/to/vault";
  input.disabled = isBusy;
  input.style.cssText = `
    flex: 1;
    padding: 8px 12px;
    border-radius: 6px;
    border: 1px solid #d1d5db;
    font-size: 13px;
  `;
  inputWrap.appendChild(input);

  const btn = document.createElement("button");
  btn.textContent = isBusy ? "Opening..." : "Open Vault";
  btn.disabled = isBusy;
  btn.style.cssText = `
    padding: 8px 16px;
    border-radius: 6px;
    border: none;
    background: #2563eb;
    color: #fff;
    cursor: pointer;
    font-size: 13px;
    opacity: ${isBusy ? "0.7" : "1"};
  `;
  btn.addEventListener("click", () => {
    const path = input.value.trim();
    if (path && typeof onOpenVault === "function") {
      onOpenVault(path);
    }
  });
  inputWrap.appendChild(btn);
  page.appendChild(inputWrap);

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

    page.appendChild(errorBlock);
  }

  return page;
}
