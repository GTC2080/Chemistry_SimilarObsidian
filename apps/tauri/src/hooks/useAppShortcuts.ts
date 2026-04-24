import { useEffect } from "react";

interface AppShortcutsOptions {
  vaultLoaded: boolean;
  onOpenSearch: () => void;
  onOpenGraph: () => void;
  onToggleAI: () => void;
  onOpenSettings: () => void;
}

export function useAppShortcuts({
  vaultLoaded,
  onOpenSearch,
  onOpenGraph,
  onToggleAI,
  onOpenSettings,
}: AppShortcutsOptions) {
  useEffect(() => {
    function handleKeyDown(e: KeyboardEvent) {
      if ((e.ctrlKey || e.metaKey) && e.key === "k") {
        e.preventDefault();
        onOpenSearch();
      }
      if ((e.ctrlKey || e.metaKey) && e.key === "g") {
        e.preventDefault();
        if (vaultLoaded) onOpenGraph();
      }
      if ((e.ctrlKey || e.metaKey) && e.key === "j") {
        e.preventDefault();
        onToggleAI();
      }
      if ((e.ctrlKey || e.metaKey) && e.key === ",") {
        e.preventDefault();
        onOpenSettings();
      }
    }

    window.addEventListener("keydown", handleKeyDown);
    return () => window.removeEventListener("keydown", handleKeyDown);
  }, [vaultLoaded, onOpenSearch, onOpenGraph, onToggleAI, onOpenSettings]);
}
