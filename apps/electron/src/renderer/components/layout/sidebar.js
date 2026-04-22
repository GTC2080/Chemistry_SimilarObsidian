/**
 * sidebar.js
 *
 * Left sidebar for workspace: Files / Search / Attachments / Chemistry / Diagnostics.
 */

const ITEMS = [
  { id: "files", label: "Files", icon: "📁" },
  { id: "search", label: "Search", icon: "🔍" },
  { id: "attachments", label: "Attachments", icon: "📎" },
  { id: "chemistry", label: "Chemistry", icon: "⚗" },
  { id: "diagnostics", label: "Diagnostics", icon: "🔧" }
];

export function createSidebar(currentPage, opts = {}) {
  const { onNavigate } = opts;

  const sidebar = document.createElement("aside");
  sidebar.className = "workspace-sidebar";
  sidebar.style.cssText = `
    width: 200px;
    border-right: 1px solid #e5e7eb;
    background: #f9fafb;
    padding: 12px;
    display: flex;
    flex-direction: column;
    gap: 2px;
    flex-shrink: 0;
    overflow: auto;
  `;

  for (let i = 0; i < ITEMS.length; i++) {
    const item = ITEMS[i];

    // Separator before diagnostics
    if (item.id === "diagnostics") {
      const sep = document.createElement("div");
      sep.style.cssText = "height: 1px; background: #e5e7eb; margin: 6px 0;";
      sidebar.appendChild(sep);
    }

    const btn = document.createElement("button");
    btn.textContent = `${item.icon} ${item.label}`;
    const active = currentPage === item.id;
    btn.style.cssText = sidebarItemStyle(active);
    btn.addEventListener("click", () => onNavigate?.(item.id));
    sidebar.appendChild(btn);
  }

  return sidebar;
}

function sidebarItemStyle(active) {
  const base = `
    padding: 8px 12px;
    border-radius: 6px;
    border: none;
    background: transparent;
    cursor: pointer;
    font-size: 13px;
    text-align: left;
    color: #374151;
    display: flex;
    align-items: center;
    gap: 8px;
    width: 100%;
  `;
  if (active) {
    return base + `
      background: #e5e7eb;
      font-weight: 600;
      box-shadow: inset 2px 0 0 0 #111827;
    `;
  }
  return base;
}
