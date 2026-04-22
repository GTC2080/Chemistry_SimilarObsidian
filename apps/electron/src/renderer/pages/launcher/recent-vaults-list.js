/**
 * recent-vaults-list.js
 *
 * Recent vaults list from localStorage.
 * Each item shows vault name (primary) + path (secondary).
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

function extractVaultName(path) {
  if (!path || typeof path !== "string") return "Unknown";
  const sep = path.includes("\\") ? "\\" : "/";
  const parts = path.split(sep).filter(Boolean);
  return parts.length > 0 ? parts[parts.length - 1] : path;
}

export function createRecentVaultsList(opts = {}) {
  const { onOpen } = opts;
  const vaults = readRecentVaults();

  const container = document.createElement("div");
  container.className = "recent-vaults-list";
  container.style.cssText = "margin-bottom: 20px;";

  const heading = document.createElement("div");
  heading.style.cssText = "font-weight: 600; font-size: 13px; color: #6b7280; margin-bottom: 10px; text-transform: uppercase; letter-spacing: 0.03em;";
  heading.textContent = "Recent Vaults";
  container.appendChild(heading);

  if (vaults.length === 0) {
    const empty = document.createElement("div");
    empty.style.cssText = `
      padding: 20px;
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
    const name = extractVaultName(path);

    const row = document.createElement("button");
    row.style.cssText = `
      display: flex;
      flex-direction: column;
      gap: 2px;
      padding: 10px 12px;
      border-radius: 8px;
      border: 1px solid #e5e7eb;
      background: #fff;
      cursor: pointer;
      font-size: 13px;
      text-align: left;
      color: #111827;
      width: 100%;
      transition: background 0.15s ease, border-color 0.15s ease;
    `;

    row.addEventListener("mouseenter", () => {
      row.style.background = "#f9fafb";
      row.style.borderLeft = "2px solid #111827";
      row.style.paddingLeft = "10px";
    });
    row.addEventListener("mouseleave", () => {
      row.style.background = "#fff";
      row.style.borderLeft = "1px solid #e5e7eb";
      row.style.paddingLeft = "12px";
    });
    row.addEventListener("mousedown", () => {
      row.style.background = "#f3f4f6";
    });
    row.addEventListener("mouseup", () => {
      row.style.background = "#f9fafb";
    });
    row.addEventListener("click", () => onOpen?.(path));

    const topLine = document.createElement("div");
    topLine.style.cssText = "display: flex; align-items: center; gap: 8px;";

    const icon = document.createElement("span");
    icon.textContent = "📁";
    icon.style.cssText = "font-size: 14px; flex-shrink: 0;";
    topLine.appendChild(icon);

    const nameEl = document.createElement("span");
    nameEl.style.cssText = "font-weight: 600; font-size: 14px; color: #111827;";
    nameEl.textContent = name;
    topLine.appendChild(nameEl);

    row.appendChild(topLine);

    const pathEl = document.createElement("div");
    pathEl.style.cssText = `
      font-size: 12px;
      color: #6b7280;
      padding-left: 22px;
      white-space: nowrap;
      overflow: hidden;
      text-overflow: ellipsis;
    `;
    pathEl.textContent = path;
    row.appendChild(pathEl);

    list.appendChild(row);
  }

  container.appendChild(list);
  return container;
}
