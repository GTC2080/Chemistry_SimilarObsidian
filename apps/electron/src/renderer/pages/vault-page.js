/**
 * vault-page.js
 *
 * Main workspace after vault open.
 * Renders the active sub-page (search, attachments, chemistry, diagnostics)
 * or a default dashboard when no sub-page is selected.
 */

import { createStateSurface } from "../components/shared/state-surface.js";

export function createVaultPage(opts = {}) {
  const { currentPage, children } = opts;

  const page = document.createElement("div");
  page.className = "vault-page";
  page.style.cssText = "height: 100%;";

  if (children) {
    page.appendChild(children);
  } else {
    const placeholder = document.createElement("div");
    placeholder.style.cssText = `
      padding: 24px;
      background: #fff;
      border-radius: 8px;
      border: 1px solid #e5e7eb;
    `;

    const title = document.createElement("h3");
    title.style.cssText = "margin: 0 0 8px;";
    title.textContent = pageTitle(currentPage);
    placeholder.appendChild(title);

    const desc = document.createElement("p");
    desc.style.cssText = "margin: 0; color: #6b7280; font-size: 13px;";
    desc.textContent = pageDescription(currentPage);
    placeholder.appendChild(desc);

    page.appendChild(placeholder);
  }

  return page;
}

function pageTitle(pageId) {
  const titles = {
    vault: "Dashboard",
    search: "Search",
    attachments: "Attachments",
    chemistry: "Chemistry",
    diagnostics: "Diagnostics"
  };
  return titles[pageId] || "Vault";
}

function pageDescription(pageId) {
  const desc = {
    vault: "Vault is open. Select a section from the navigation.",
    search: "Search across notes and attachments.",
    attachments: "Browse attachments and PDF metadata.",
    chemistry: "Browse chemistry spectra and metadata.",
    diagnostics: "Export diagnostics and manage rebuilds."
  };
  return desc[pageId] || "";
}
