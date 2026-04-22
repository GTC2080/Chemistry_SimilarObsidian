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
    console.log("renderer.app.bootstrap.effect");
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
    console.log("renderer.app.bootstrap.start");
    setBooting(true);

    const [bootstrapEnvelope, sessionEnvelope, desktopVersion] = await Promise.all([
      getBootstrapInfo(),
      getSessionStatus(),
      getDesktopAppVersion()
    ]);

    console.log("renderer.app.bootstrap.responses", {
      bootstrapOk: bootstrapEnvelope?.ok ?? null,
      sessionOk: sessionEnvelope?.ok ?? null,
      desktopVersion: desktopVersion ?? null,
      sessionState: sessionEnvelope?.data?.state ?? null
    });

    if (!bootstrapEnvelope?.ok) {
      console.log("renderer.app.bootstrap.host_unavailable");
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
      console.log("renderer.app.bootstrap.enter_workspace_from_session", {
        vaultPath: sessionEnvelope.data.active_vault_path
      });
      await enterWorkspace(sessionEnvelope.data.active_vault_path);
    } else {
      console.log("renderer.app.bootstrap.launcher");
      setLauncherState("no_vault_open");
    }

    setBooting(false);
    console.log("renderer.app.bootstrap.done");
  }

  async function enterWorkspace(nextVaultPath: string) {
    console.log("renderer.app.enterWorkspace.start", { nextVaultPath });
    const treeEnvelope = await scanVaultTree();
    const recent = await loadRecentNotes();

    console.log("renderer.app.enterWorkspace.responses", {
      treeOk: treeEnvelope.ok,
      recentCount: recent.length
    });

    setVaultPath(nextVaultPath);
    setRecentNotes(recent);

    if (treeEnvelope.ok) {
      setTree(treeEnvelope.data.tree);
      setNotes(treeEnvelope.data.notes);
      if (treeEnvelope.data.notes.length > 0) {
        console.log("renderer.app.enterWorkspace.open_first_note", {
          relPath: treeEnvelope.data.notes[0].relPath
        });
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

    console.log("renderer.app.enterWorkspace.done");
  }

  async function openNote(relPath: string, notePool = notes) {
    console.log("renderer.app.openNote.start", { relPath });
    const target = notePool.find((note) => note.relPath === relPath) ?? null;
    setActiveNote(target);
    setContentLoading(true);
    setContentError(null);

    const envelope = await loadNoteContent(relPath);
    console.log("renderer.app.openNote.response", {
      ok: envelope?.ok ?? null,
      relPath
    });
    if (envelope?.ok && envelope.data) {
      setNoteBody(envelope.data.bodyText ?? "");
    } else {
      setNoteBody("");
      setContentError(envelope?.error?.message ?? "Failed to load note content.");
    }

    setContentLoading(false);
    console.log("renderer.app.openNote.done", { relPath });
  }

  async function openVaultPath(targetPath: string) {
    console.log("renderer.app.openVaultPath.start", { targetPath });
    if (!targetPath) {
      return;
    }

    setLauncherState("opening_vault");
    setLastError(null);

    const result = await openVault(targetPath);
    console.log("renderer.app.openVaultPath.response", {
      ok: result?.ok ?? null,
      targetPath
    });
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
    console.log("renderer.app.openVaultPath.done", { targetPath });
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
