/**
 * recent-vaults-list.js
 *
 * Recent vaults list from localStorage.
 * Styled as the left launcher rail.
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
  const { onOpen, disabled = false } = opts;
  const vaults = readRecentVaults();

  const container = document.createElement("div");
  container.className = "recent-vaults-list";
  container.style.cssText = `
    display: flex;
    flex-direction: column;
    min-height: 100%;
  `;

  const railTitle = document.createElement("div");
  railTitle.style.cssText = "font-size: 12px; color: #8d90a2; text-transform: uppercase; letter-spacing: 0.14em; margin-bottom: 18px;";
  railTitle.textContent = "Recent Vaults";
  container.appendChild(railTitle);

  const heading = document.createElement("div");
  heading.style.cssText = "font-weight: 600; font-size: 12px; color: #6b7280; margin-bottom: 10px; text-transform: uppercase; letter-spacing: 0.03em; display:none;";
  heading.textContent = "Recent Vaults";
  container.appendChild(heading);

  if (vaults.length === 0) {
    const empty = document.createElement("div");
    empty.style.cssText = `
      padding: 18px 16px;
      border-radius: 16px;
      background: rgba(255,255,255,0.04);
      border: 1px solid rgba(255,255,255,0.07);
      color: #8d90a2;
      font-size: 13px;
      line-height: 1.6;
    `;
    empty.textContent = "No recent vaults yet. Open a local vault from the right side to start building your workspace list.";
    container.appendChild(empty);
    return container;
  }

  const list = document.createElement("div");
  list.style.cssText = "display: flex; flex-direction: column; gap: 8px;";

  for (const path of vaults) {
    const name = extractVaultName(path);

    const row = document.createElement("button");
    row.style.cssText = `
      display: flex;
      align-items: flex-start;
      justify-content: space-between;
      gap: 12px;
      padding: 12px 13px 12px;
      border-radius: 16px;
      border: 1px solid rgba(255,255,255,0.08);
      background: rgba(255,255,255,0.03);
      cursor: ${disabled ? "not-allowed" : "pointer"};
      font-size: 13px;
      text-align: left;
      color: #f5f5f5;
      width: 100%;
      transition: background 0.15s ease, border-color 0.15s ease, transform 0.15s ease;
      opacity: ${disabled ? "0.58" : "1"};
    `;

    row.addEventListener("mouseenter", () => {
      if (disabled) return;
      row.style.background = "rgba(255,255,255,0.06)";
      row.style.borderColor = "rgba(144, 116, 255, 0.45)";
      row.style.transform = "translateX(2px)";
    });
    row.addEventListener("mouseleave", () => {
      if (disabled) return;
      row.style.background = "rgba(255,255,255,0.03)";
      row.style.borderColor = "rgba(255,255,255,0.08)";
      row.style.transform = "translateX(0)";
    });
    row.addEventListener("mousedown", () => {
      if (disabled) return;
      row.style.background = "rgba(255,255,255,0.08)";
    });
    row.addEventListener("mouseup", () => {
      if (disabled) return;
      row.style.background = "rgba(255,255,255,0.06)";
    });
    row.addEventListener("click", () => {
      if (disabled) return;
      onOpen?.(path);
    });

    const content = document.createElement("div");
    content.style.cssText = "min-width: 0; flex: 1;";

    const topLine = document.createElement("div");
    topLine.style.cssText = "display: flex; align-items: center; gap: 8px; min-width: 0;";

    const icon = document.createElement("span");
    icon.textContent = "▣";
    icon.style.cssText = "font-size: 12px; color: #9b88ff; flex-shrink: 0;";
    topLine.appendChild(icon);

    const nameEl = document.createElement("span");
    nameEl.style.cssText = "font-weight: 600; font-size: 14px; color: #f5f5f5; white-space: nowrap; overflow: hidden; text-overflow: ellipsis;";
    nameEl.textContent = name;
    topLine.appendChild(nameEl);

    content.appendChild(topLine);

    const pathEl = document.createElement("div");
    pathEl.style.cssText = `
      font-size: 11px;
      color: #8d90a2;
      margin-top: 6px;
      line-height: 1.45;
      word-break: break-all;
    `;
    pathEl.textContent = path;
    content.appendChild(pathEl);

    row.appendChild(content);

    const actionHint = document.createElement("span");
    actionHint.textContent = "Open";
    actionHint.style.cssText = "color: #a89dce; font-size: 11px; line-height: 1; padding-top: 5px; letter-spacing: 0.12em; text-transform: uppercase;";
    row.appendChild(actionHint);

    list.appendChild(row);
  }

  container.appendChild(list);

  const footer = document.createElement("div");
  footer.style.cssText = "margin-top: auto; padding-top: 18px; font-size: 11px; color: #747789; line-height: 1.55;";
  footer.textContent = "Select a recent vault to reopen it instantly, or use the actions on the right to open a different local vault.";
  container.appendChild(footer);

  return container;
}
