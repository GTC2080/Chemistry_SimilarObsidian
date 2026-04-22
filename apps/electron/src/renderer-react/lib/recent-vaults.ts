const STORAGE_KEY = "nexus-launcher-recent-vaults";

export interface RecentVault {
  name: string;
  path: string;
}

function deriveName(vaultPath: string) {
  return vaultPath.replace(/[\\/]+$/, "").split(/[\\/]/).pop() || "Vault";
}

export function readRecentVaults(): RecentVault[] {
  try {
    const raw = window.localStorage.getItem(STORAGE_KEY);
    if (!raw) {
      return [];
    }

    const parsed = JSON.parse(raw);
    if (!Array.isArray(parsed)) {
      return [];
    }

    return parsed.filter((item) => item && typeof item.path === "string").map((item) => ({
      name: typeof item.name === "string" && item.name.trim() ? item.name : deriveName(item.path),
      path: item.path
    }));
  } catch {
    return [];
  }
}

function writeRecentVaults(items: RecentVault[]) {
  window.localStorage.setItem(STORAGE_KEY, JSON.stringify(items.slice(0, 8)));
}

export function addRecentVault(vaultPath: string) {
  const next = readRecentVaults().filter((item) => item.path !== vaultPath);
  next.unshift({
    name: deriveName(vaultPath),
    path: vaultPath
  });
  writeRecentVaults(next);
}

export function removeRecentVault(vaultPath: string) {
  writeRecentVaults(readRecentVaults().filter((item) => item.path !== vaultPath));
}
