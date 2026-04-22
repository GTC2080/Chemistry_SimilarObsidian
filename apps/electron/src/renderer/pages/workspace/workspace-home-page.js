/**
 * workspace-home-page.js
 *
 * Default workspace view when Files is selected (placeholder).
 */

export function createWorkspaceHomePage(opts = {}) {
  const { vaultPath } = opts;

  const page = document.createElement("div");
  page.className = "workspace-home-page";

  const card = document.createElement("div");
  card.style.cssText = `
    padding: 24px;
    background: #fff;
    border-radius: 8px;
    border: 1px solid #e5e7eb;
  `;

  const title = document.createElement("h3");
  title.style.cssText = "margin: 0 0 8px; font-size: 16px;";
  title.textContent = "Files";
  card.appendChild(title);

  const desc = document.createElement("p");
  desc.style.cssText = "margin: 0 0 12px; color: #6b7280; font-size: 13px;";
  desc.textContent = "Files view coming soon. Select another section from the sidebar.";
  card.appendChild(desc);

  if (vaultPath) {
    const meta = document.createElement("div");
    meta.style.cssText = "font-size: 12px; color: #9ca3af; font-family: monospace;";
    meta.textContent = vaultPath;
    card.appendChild(meta);
  }

  page.appendChild(card);
  return page;
}
