/**
 * nav-bar.js
 *
 * Minimal navigation bar.
 */

const PAGE_LINKS = [
  { id: "search", label: "Search" },
  { id: "attachments", label: "Attachments" },
  { id: "chemistry", label: "Chemistry" },
  { id: "diagnostics", label: "Diagnostics" }
];

export function createNavBar(currentPage, opts = {}) {
  const { onNavigate, sessionOpen } = opts;

  const nav = document.createElement("nav");
  nav.className = "nav-bar";
  nav.style.cssText = `
    display: flex;
    gap: 4px;
    padding: 8px 16px;
    border-bottom: 1px solid #e5e7eb;
    background: #f9fafb;
    align-items: center;
  `;

  const home = document.createElement("button");
  home.textContent = "Vault";
  home.style.cssText = linkStyle(currentPage === "vault");
  home.addEventListener("click", () => onNavigate?.("vault"));
  nav.appendChild(home);

  const separator = document.createElement("span");
  separator.textContent = "|";
  separator.style.cssText = "color: #d1d5db; margin: 0 4px;";
  nav.appendChild(separator);

  for (const link of PAGE_LINKS) {
    const btn = document.createElement("button");
    btn.textContent = link.label;
    btn.disabled = !sessionOpen;
    btn.style.cssText = linkStyle(currentPage === link.id, !sessionOpen);
    btn.addEventListener("click", () => {
      if (sessionOpen) onNavigate?.(link.id);
    });
    nav.appendChild(btn);
  }

  return nav;
}

function linkStyle(active, disabled) {
  const base = `
    padding: 4px 10px;
    border-radius: 6px;
    border: none;
    background: transparent;
    cursor: pointer;
    font-size: 13px;
    color: #374151;
  `;
  if (disabled) {
    return base + " opacity: 0.4; cursor: not-allowed;";
  }
  if (active) {
    return base + " background: #e5e7eb; font-weight: 600;";
  }
  return base;
}
