import { useCallback, useEffect, useRef, useState } from "react";
import { LazyStore } from "@tauri-apps/plugin-store";
import type { RecentVault } from "../types/vault";

const vaultStore = new LazyStore("vaults.json");

export function useRecentVaults() {
  const [recentVaults, setRecentVaults] = useState<RecentVault[]>([]);
  const recentVaultsRef = useRef<RecentVault[]>([]);

  useEffect(() => {
    vaultStore
      .get<RecentVault[]>("recentVaults")
      .then(list => {
        const loaded = Array.isArray(list) ? list : [];
        recentVaultsRef.current = loaded;
        setRecentVaults(loaded);
      })
      .catch(() => {});
  }, []);

  const saveToRecent = useCallback(async (path: string) => {
    const name = path.split(/[/\\]/).pop() || "Vault";
    const entry: RecentVault = { name, path, openedAt: Date.now() };
    const updated = [entry, ...recentVaultsRef.current.filter(v => v.path !== path)].slice(0, 10);
    recentVaultsRef.current = updated;
    setRecentVaults(updated);
    await vaultStore.set("recentVaults", updated);
    await vaultStore.save();
  }, []);

  const removeFromRecent = useCallback(async (path: string) => {
    const updated = recentVaultsRef.current.filter(v => v.path !== path);
    if (updated.length === recentVaultsRef.current.length) return;
    recentVaultsRef.current = updated;
    setRecentVaults(updated);
    await vaultStore.set("recentVaults", updated);
    await vaultStore.save();
  }, []);

  return { recentVaults, saveToRecent, removeFromRecent };
}
