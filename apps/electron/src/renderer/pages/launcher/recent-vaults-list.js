/**
 * recent-vaults-list.js
 *
 * Recent vaults list from localStorage.
 */

const STORAGE_KEY = "chem_obsidian_recent_vaults";
const MAX_ITEMS = 10;

export function readRecentVaults() {
  try {
    const raw = localStorage.getItem(STORAGE_KEY);
    if (!raw) return [];
    const parsed = JSON.parse(raw);
    if (Array.isArray(parsed)) return parsed.slice(0, MAX_ITEMS);
    return [];
  } catch {
    return [];
  }
}

export function addRecentVault(vaultPath) {
  if (!vaultPath || typeof vaultPath !== "string") return;
  const list = readRecentVaults().filter((p) => p !== vaultPath);
  list.unshift(vaultPath);
  try {
    localStorage.setItem(STORAGE_KEY, JSON.stringify(list.slice(0, MAX_ITEMS)));
  } catch {
    // ignore
  }
}

export function createRecentVaultsList(opts = {}) {
  const { onOpen, onClear } = opts;
  const vaults = readRecentVaults();

  const container = document.createElement("div");
  container.className = "recent-vaults-list";
  container.style.cssText = "margin-bottom: 20px;";

  const heading = document.createElement("div");
  heading.style.cssText = "font-weight: 600; font-size: 13px; color: #6b7280; margin-bottom: 8px; text-transform: uppercase; letter-spacing: 0.03em;";
  heading.textContent = "Recent Vaults";
  container.appendChild(heading);

  if (vaults.length === 0) {
    const empty = document.createElement("div");
    empty.style.cssText = `
      padding: 16px;
      border-radius: 8px;
      background: #f9fafb;
      color: #9ca3af;
      font-size: 13px;
      text-align: center;
    `;
    empty.textContent = "No recent vaults. Open or create one to get started.";
    container.appendChild(empty);
    return container;
  }

  const list = document.createElement("div");
  list.style.cssText = "display: flex; flex-direction: column; gap: 6px;";

  for (const path of vaults) {
    const row = document.createElement("button");
    row.style.cssText = `
      display: flex;
      align-items: center;
      gap: 10px;
      padding: 10px 12px;
      border-radius: 8px;
      border: 1px solid #e5e7eb;
      background: #fff;
      cursor: pointer;
      font-size: 13px;
      text-align: left;
      color: #111827;
      width: 100%;
    `;
    row.addEventListener("mouseenter", () => { row.style.background = "#f9fafb"; });
    row.addEventListener("mouseleave", () => { row.style.background = "#fff"; });
    row.addEventListener("click", () => onOpen?.(path));

    const icon = document.createElement("span");
    icon.textContent = "📁";
    row.appendChild(icon);

    const label = document.createElement("span");
    label.style.cssText = "flex: 1; white-space: nowrap; overflow: hidden; text-overflow: ellipsis;";
    label.textContent = path;
    row.appendChild(label);

    list.appendChild(row);
  }

  container.appendChild(list);

  if (typeof onClear === "function") {
    const clearBtn = document.createElement("button");
    clearBtn.textContent = "Clear recent";
    clearBtn.style.cssText = `
      margin-top: 8px;
      border: none;
      background: transparent;
      color: #9ca3af;
      cursor: pointer;
      font-size: 12px;
    `;
    clearBtn.addEventListener("click", onClear);
    container.appendChild(clearBtn);
  }

  return container;
}
