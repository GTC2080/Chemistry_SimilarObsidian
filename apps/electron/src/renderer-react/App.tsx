import { useEffect, useMemo, useState } from "react";
import AppTitleBar from "./components/nexus/AppTitleBar";
import LaunchSplash from "./components/nexus/LaunchSplash";
import VaultManagerView from "./components/nexus/VaultManagerView";
import WorkspaceShell from "./components/workspace/WorkspaceShell";
import { getDesktopAppVersion, pickDirectory } from "./lib/desktop-shell";
import { loadNoteContent, loadRecentNotes, scanVaultTree, type NoteRecord, type TreeNode } from "./lib/files-tree";
import { addRecentVault, readRecentVaults, removeRecentVault, type RecentVault } from "./lib/recent-vaults";
import { closeVault, getBootstrapInfo, getRuntimeSummary, getSessionStatus, openVault, querySearch } from "./lib/host-shell";

type HostEnvelopeError = { code?: string; message?: string } | null;

export default function App() {
  const [booting, setBooting] = useState(true);
  const [hostUnavailable, setHostUnavailable] = useState(false);
  const [hostVersion, setHostVersion] = useState<string | null>(null);
  const [recentVaults, setRecentVaults] = useState<RecentVault[]>(() => readRecentVaults());
  const [launcherState, setLauncherState] = useState<"no_vault_open" | "opening_vault" | "host_unavailable">("no_vault_open");
  const [lastError, setLastError] = useState<HostEnvelopeError>(null);
  const [vaultPath, setVaultPath] = useState("");
  const [tree, setTree] = useState<TreeNode[]>([]);
  const [notes, setNotes] = useState<NoteRecord[]>([]);
  const [recentNotes, setRecentNotes] = useState<NoteRecord[]>([]);
  const [activeNote, setActiveNote] = useState<NoteRecord | null>(null);
  const [noteBody, setNoteBody] = useState("");
  const [contentLoading, setContentLoading] = useState(false);
  const [contentError, setContentError] = useState<string | null>(null);
  const [searchQueryValue, setSearchQueryValue] = useState("");
  const [searchResults, setSearchResults] = useState<{ relPath: string; title: string }[]>([]);
  const [searchLoading, setSearchLoading] = useState(false);

  useEffect(() => {
    const splash = document.getElementById("pre-splash");
    if (splash) {
      splash.classList.add("fade-out");
      const timer = window.setTimeout(() => splash.remove(), 400);
      return () => window.clearTimeout(timer);
    }
    return undefined;
  }, []);

  useEffect(() => {
    void bootstrap();
  }, []);

  useEffect(() => {
    if (!searchQueryValue.trim() || !vaultPath) {
      setSearchResults([]);
      setSearchLoading(false);
      return;
    }

    const timer = window.setTimeout(() => {
      void (async () => {
        setSearchLoading(true);
        const envelope = await querySearch(searchQueryValue.trim());
        if (envelope?.ok && envelope.data) {
          setSearchResults((envelope.data.items ?? []).map((item: any) => ({
            relPath: item.relPath,
            title: item.title ?? item.relPath
          })));
        } else {
          setSearchResults([]);
        }
        setSearchLoading(false);
      })();
    }, 180);

    return () => window.clearTimeout(timer);
  }, [searchQueryValue, vaultPath]);

  const appVersion = useMemo(() => hostVersion ?? "1.0.0", [hostVersion]);

  async function bootstrap() {
    setBooting(true);

    const [bootstrapEnvelope, sessionEnvelope, desktopVersion] = await Promise.all([
      getBootstrapInfo(),
      getSessionStatus(),
      getDesktopAppVersion()
    ]);

    if (!bootstrapEnvelope?.ok) {
      setHostUnavailable(true);
      setLauncherState("host_unavailable");
      setLastError(bootstrapEnvelope?.error ?? { code: "HOST_UNAVAILABLE", message: "Host bootstrap failed." });
      setBooting(false);
      return;
    }

    setHostVersion(desktopVersion ?? bootstrapEnvelope.data?.host_version ?? "1.0.0");
    setHostUnavailable(false);
    setLastError(null);

    if (sessionEnvelope?.ok && sessionEnvelope.data?.state === "open" && sessionEnvelope.data?.active_vault_path) {
      await enterWorkspace(sessionEnvelope.data.active_vault_path);
    } else {
      setLauncherState("no_vault_open");
    }

    setBooting(false);
  }

  async function enterWorkspace(nextVaultPath: string) {
    const treeEnvelope = await scanVaultTree();
    const recent = await loadRecentNotes();

    setVaultPath(nextVaultPath);
    setRecentNotes(recent);

    if (treeEnvelope.ok) {
      setTree(treeEnvelope.data.tree);
      setNotes(treeEnvelope.data.notes);
      if (treeEnvelope.data.notes.length > 0) {
        await openNote(treeEnvelope.data.notes[0].relPath, treeEnvelope.data.notes);
      } else {
        setActiveNote(null);
        setNoteBody("");
      }
    } else {
      setTree([]);
      setNotes([]);
      setActiveNote(null);
      setNoteBody("");
      setLastError(treeEnvelope.error);
    }
  }

  async function openNote(relPath: string, notePool = notes) {
    const target = notePool.find((note) => note.relPath === relPath) ?? null;
    setActiveNote(target);
    setContentLoading(true);
    setContentError(null);

    const envelope = await loadNoteContent(relPath);
    if (envelope?.ok && envelope.data) {
      setNoteBody(envelope.data.bodyText ?? "");
    } else {
      setNoteBody("");
      setContentError(envelope?.error?.message ?? "Failed to load note content.");
    }

    setContentLoading(false);
  }

  async function openVaultPath(targetPath: string) {
    if (!targetPath) {
      return;
    }

    setLauncherState("opening_vault");
    setLastError(null);

    const result = await openVault(targetPath);
    if (!result?.ok) {
      setLauncherState("no_vault_open");
      setLastError(result?.error ?? { code: "HOST_OPEN_FAILED", message: "Failed to open vault." });
      return;
    }

    addRecentVault(targetPath);
    setRecentVaults(readRecentVaults());
    await getRuntimeSummary();
    await enterWorkspace(targetPath);
    setLauncherState("no_vault_open");
  }

  async function handleOpenVault() {
    const selected = await pickDirectory();
    if (!selected) {
      return;
    }
    await openVaultPath(selected);
  }

  async function handleBackToManager() {
    await closeVault();
    setVaultPath("");
    setTree([]);
    setNotes([]);
    setRecentNotes([]);
    setActiveNote(null);
    setNoteBody("");
    setSearchQueryValue("");
    setSearchResults([]);
    setLauncherState("no_vault_open");
  }

  if (booting) {
    return (
      <div className="h-screen w-screen workspace-canvas flex flex-col">
        <AppTitleBar />
        <LaunchSplash />
      </div>
    );
  }

  return (
    <div className="h-screen w-screen workspace-canvas flex flex-col overflow-hidden">
      <AppTitleBar />

      {!vaultPath || hostUnavailable ? (
        <VaultManagerView
          recentVaults={recentVaults}
          appVersion={appVersion}
          launcherState={hostUnavailable ? "host_unavailable" : launcherState}
          lastError={lastError}
          onOpenRecent={openVaultPath}
          onRemoveRecent={(path) => {
            removeRecentVault(path);
            setRecentVaults(readRecentVaults());
          }}
          onOpenVault={handleOpenVault}
        />
      ) : (
        <WorkspaceShell
          vaultPath={vaultPath}
          tree={tree}
          notes={notes}
          recentNotes={recentNotes}
          activeNote={activeNote}
          noteBody={noteBody}
          contentLoading={contentLoading}
          contentError={contentError}
          searchQuery={searchQueryValue}
          searchResults={searchResults}
          searchLoading={searchLoading}
          onBackToManager={handleBackToManager}
          onSelectNote={(relPath) => {
            void openNote(relPath);
          }}
          onClearNote={() => {
            setActiveNote(null);
            setNoteBody("");
            setContentError(null);
            setContentLoading(false);
          }}
          onSearchQueryChange={setSearchQueryValue}
        />
      )}
    </div>
  );
}
