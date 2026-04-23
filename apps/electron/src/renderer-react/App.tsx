import { useEffect, useMemo, useState } from "react";
import AppTitleBar from "./components/nexus/AppTitleBar";
import LaunchSplash from "./components/nexus/LaunchSplash";
import VaultManagerView from "./components/nexus/VaultManagerView";
import TextInputDialog from "./components/workspace/TextInputDialog";
import WorkspaceShell from "./components/workspace/WorkspaceShell";
import { getDesktopAppVersion, pickDirectory } from "./lib/desktop-shell";
import { createFolderInVault, createNoteRecord, deleteVaultEntry, loadNoteContent, loadRecentNotes, renameVaultEntry, saveNoteContent, scanVaultTree, type NoteRecord, type TreeNode } from "./lib/files-tree";
import { addRecentVault, readRecentVaults, removeRecentVault, type RecentVault } from "./lib/recent-vaults";
import { closeVault, getBootstrapInfo, getRuntimeSummary, getSessionStatus, openVault, querySearch } from "./lib/host-shell";

type HostEnvelopeError = { code?: string; message?: string } | null;
type SaveState = "idle" | "saving" | "saved" | "error";
type TextDialogState = {
  title: string;
  message?: string;
  defaultValue?: string;
  confirmLabel?: string;
  resolve: (value: string | null) => void;
} | null;

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
  const [savedNoteBody, setSavedNoteBody] = useState("");
  const [noteRevision, setNoteRevision] = useState<string | null>(null);
  const [saveState, setSaveState] = useState<SaveState>("idle");
  const [saveError, setSaveError] = useState<string | null>(null);
  const [fileOperationError, setFileOperationError] = useState<string | null>(null);
  const [contentLoading, setContentLoading] = useState(false);
  const [contentError, setContentError] = useState<string | null>(null);
  const [searchQueryValue, setSearchQueryValue] = useState("");
  const [searchResults, setSearchResults] = useState<{ relPath: string; title: string }[]>([]);
  const [searchLoading, setSearchLoading] = useState(false);
  const [textDialog, setTextDialog] = useState<TextDialogState>(null);

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
  const noteDirty = Boolean(activeNote && noteBody !== savedNoteBody);

  useEffect(() => {
    function handleBeforeUnload(event: BeforeUnloadEvent) {
      if (!noteDirty) {
        return;
      }

      event.preventDefault();
      event.returnValue = "";
    }

    window.addEventListener("beforeunload", handleBeforeUnload);
    return () => window.removeEventListener("beforeunload", handleBeforeUnload);
  }, [noteDirty]);

  useEffect(() => {
    function handleKeyDown(event: KeyboardEvent) {
      if (!(event.ctrlKey || event.metaKey)) {
        return;
      }

      const key = event.key.toLowerCase();
      if (key === "s") {
        event.preventDefault();
        if (activeNote && noteDirty && saveState !== "saving") {
          void handleSaveNote();
        }
        return;
      }

      if (key === "n" && vaultPath && saveState !== "saving") {
        event.preventDefault();
        void handleCreateNote();
        return;
      }
    }

    window.addEventListener("keydown", handleKeyDown);
    return () => window.removeEventListener("keydown", handleKeyDown);
  }, [activeNote, noteDirty, noteBody, noteRevision, saveState, vaultPath]);

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
        setSavedNoteBody("");
        setNoteRevision(null);
        setSaveState("idle");
        setSaveError(null);
      }
    } else {
      setTree([]);
      setNotes([]);
      setActiveNote(null);
      setNoteBody("");
      setSavedNoteBody("");
      setNoteRevision(null);
      setSaveState("idle");
      setSaveError(null);
      setLastError(treeEnvelope.error);
    }
  }

  function confirmDiscardUnsaved() {
    if (!noteDirty) {
      return true;
    }

    return window.confirm("当前笔记还有未保存修改。是否放弃这些修改并继续？");
  }

  async function openNote(relPath: string, notePool = notes) {
    if (activeNote?.relPath !== relPath && !confirmDiscardUnsaved()) {
      return;
    }

    const target = notePool.find((note) => note.relPath === relPath) ?? null;
    setActiveNote(target);
    setContentLoading(true);
    setContentError(null);

    const envelope = await loadNoteContent(relPath);
    if (envelope?.ok && envelope.data) {
      setNoteBody(envelope.data.bodyText ?? "");
      setSavedNoteBody(envelope.data.bodyText ?? "");
      setNoteRevision(envelope.data.contentRevision || null);
      setSaveState("idle");
      setSaveError(null);
    } else {
      setNoteBody("");
      setSavedNoteBody("");
      setNoteRevision(null);
      setSaveState("idle");
      setSaveError(null);
      setContentError(envelope?.error?.message ?? "Failed to load note content.");
    }

    setContentLoading(false);
  }

  async function refreshContentLists(preferredRelPath?: string) {
    const treeEnvelope = await scanVaultTree();
    const recent = await loadRecentNotes();
    setRecentNotes(recent);

    if (!treeEnvelope.ok) {
      setLastError(treeEnvelope.error);
      return [];
    }

    setTree(treeEnvelope.data.tree);
    setNotes(treeEnvelope.data.notes);
    if (preferredRelPath) {
      const refreshedNote = treeEnvelope.data.notes.find((note) => note.relPath === preferredRelPath);
      if (refreshedNote) {
        setActiveNote(refreshedNote);
      }
    }
    return treeEnvelope.data.notes;
  }

  function nextUntitledRelPath(parentRelPath?: string) {
    const existing = new Set(notes.map((note) => note.relPath.toLowerCase()));
    const activeFolder = activeNote?.relPath.includes("/")
      ? activeNote.relPath.split("/").slice(0, -1).join("/")
      : "";
    const hasNotesFolder = tree.some((node) => node.isFolder && node.relPath.toLowerCase() === "notes");
    const folder = parentRelPath ?? (activeFolder || (hasNotesFolder ? "notes" : ""));

    for (let index = 1; index < 1000; index += 1) {
      const name = index === 1 ? "新建笔记.md" : `新建笔记 ${index}.md`;
      const relPath = folder ? `${folder}/${name}` : name;
      if (!existing.has(relPath.toLowerCase())) {
        return relPath;
      }
    }

    return folder ? `${folder}/新建笔记 ${Date.now()}.md` : `新建笔记 ${Date.now()}.md`;
  }

  function describeSaveFailure(error: HostEnvelopeError, fallback: string) {
    if (error?.code === "HOST_KERNEL_CONFLICT") {
      return "保存失败：这篇笔记已在外部发生变化。请重新打开后再保存，避免覆盖外部修改。";
    }
    if (error?.code === "HOST_KERNEL_NOT_FOUND") {
      return "保存失败：目标笔记不存在或已被移动。请刷新文件列表后再试。";
    }
    if (error?.code === "HOST_SESSION_NOT_OPEN") {
      return "保存失败：当前没有打开的 vault。";
    }

    return error?.message ?? fallback;
  }

  async function handleCreateNote(parentRelPath?: string) {
    if (!confirmDiscardUnsaved()) {
      return;
    }

    const relPath = nextUntitledRelPath(parentRelPath);
    const bodyText = "# 新建笔记\n\n";
    setSaveState("saving");
    setSaveError(null);
    setFileOperationError(null);

    const envelope = await saveNoteContent(relPath, bodyText, null);
    if (envelope?.ok && envelope.data?.note) {
      const noteRecord = createNoteRecord(envelope.data.note);
      setActiveNote(noteRecord);
      setNoteBody(envelope.data.note.bodyText ?? bodyText);
      setSavedNoteBody(envelope.data.note.bodyText ?? bodyText);
      setNoteRevision(envelope.data.note.contentRevision || null);
      setSaveState("saved");
      setSaveError(null);
      await refreshContentLists(noteRecord.relPath);
      return;
    }

    setSaveState("error");
    setSaveError(describeSaveFailure(envelope?.error ?? null, "Failed to create note."));
  }

  async function handleSaveNote() {
    if (!activeNote || saveState === "saving") {
      return;
    }

    setSaveState("saving");
    setSaveError(null);
    setFileOperationError(null);

    const envelope = await saveNoteContent(activeNote.relPath, noteBody, noteRevision);
    if (envelope?.ok && envelope.data?.note) {
      const noteRecord = createNoteRecord(envelope.data.note);
      const nextBody = envelope.data.note.bodyText ?? noteBody;
      setActiveNote(noteRecord);
      setNoteBody(nextBody);
      setSavedNoteBody(nextBody);
      setNoteRevision(envelope.data.note.contentRevision || null);
      setSaveState("saved");
      setSaveError(null);
      await refreshContentLists(noteRecord.relPath);
      return;
    }

    setSaveState("error");
    setSaveError(describeSaveFailure(envelope?.error ?? null, "Failed to save note."));
  }

  function activeFolderRelPath() {
    if (!activeNote?.relPath.includes("/")) {
      return "";
    }

    return activeNote.relPath.split("/").slice(0, -1).join("/");
  }

  function requestTextInput(options: Omit<NonNullable<TextDialogState>, "resolve">) {
    return new Promise<string | null>((resolve) => {
      setTextDialog({
        ...options,
        resolve
      });
    });
  }

  function closeTextDialog(value: string | null) {
    if (textDialog) {
      textDialog.resolve(value);
    }
    setTextDialog(null);
  }

  async function handleCreateFolder(parentRelPath?: string) {
    const folderName = await requestTextInput({
      title: "新建文件夹",
      message: parentRelPath ? `在 ${parentRelPath} 下创建一个文件夹。` : "在当前目录下创建一个文件夹。",
      confirmLabel: "创建"
    });
    if (!folderName) {
      return;
    }

    setFileOperationError(null);
    const envelope = await createFolderInVault(parentRelPath ?? activeFolderRelPath(), folderName);
    if (envelope?.ok) {
      await refreshContentLists();
      return;
    }

    setFileOperationError(envelope?.error?.message ?? "新建文件夹失败。");
  }

  async function handleRenameNote(relPath?: string) {
    const targetRelPath = relPath ?? activeNote?.relPath;
    if (!targetRelPath) {
      return;
    }

    if (noteDirty && targetRelPath === activeNote?.relPath) {
      setFileOperationError("请先保存当前笔记，再重命名。");
      return;
    }

    const targetNote = notes.find((note) => note.relPath === targetRelPath) ?? activeNote;
    const currentName = targetNote?.name || targetRelPath.split("/").pop() || "Untitled.md";
    const nextNameInput = await requestTextInput({
      title: "重命名笔记",
      message: "只输入文件名，不需要输入路径。",
      defaultValue: currentName,
      confirmLabel: "重命名"
    });
    if (!nextNameInput || !nextNameInput.trim()) {
      return;
    }

    if (nextNameInput.includes("/") || nextNameInput.includes("\\")) {
      setFileOperationError("重命名只接受文件名；移动到其他文件夹会在后续文件树交互里处理。");
      return;
    }

    const nextName = nextNameInput.trim().endsWith(".md")
      ? nextNameInput.trim()
      : `${nextNameInput.trim()}.md`;
    const folder = targetRelPath.includes("/")
      ? targetRelPath.split("/").slice(0, -1).join("/")
      : "";
    const nextRelPath = folder ? `${folder}/${nextName}` : nextName;
    if (nextRelPath === targetRelPath) {
      return;
    }

    setFileOperationError(null);
    const envelope = await renameVaultEntry(targetRelPath, nextRelPath);
    if (envelope?.ok) {
      const refreshedNotes = await refreshContentLists(nextRelPath);
      if (targetRelPath === activeNote?.relPath) {
        await openNote(nextRelPath, refreshedNotes);
      }
      return;
    }

    setFileOperationError(envelope?.error?.message ?? "重命名失败。");
  }

  async function handleDeleteNote(relPath?: string) {
    const targetRelPath = relPath ?? activeNote?.relPath;
    if (!targetRelPath) {
      return;
    }

    const isActiveTarget = targetRelPath === activeNote?.relPath
      || Boolean(activeNote?.relPath.startsWith(`${targetRelPath}/`));
    const confirmed = window.confirm(`确认删除 ${targetRelPath}？此操作会删除磁盘文件。`);
    if (!confirmed) {
      return;
    }

    setFileOperationError(null);
    const envelope = await deleteVaultEntry(targetRelPath);
    if (envelope?.ok) {
      if (isActiveTarget) {
        setActiveNote(null);
        setNoteBody("");
        setSavedNoteBody("");
        setNoteRevision(null);
        setSaveState("idle");
        setSaveError(null);
      }
      await refreshContentLists();
      return;
    }

    setFileOperationError(envelope?.error?.message ?? "删除失败。");
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
    if (!confirmDiscardUnsaved()) {
      return;
    }

    await closeVault();
    setVaultPath("");
    setTree([]);
    setNotes([]);
    setRecentNotes([]);
    setActiveNote(null);
    setNoteBody("");
    setSavedNoteBody("");
    setNoteRevision(null);
    setSaveState("idle");
    setSaveError(null);
    setFileOperationError(null);
    setSearchQueryValue("");
    setSearchResults([]);
    setLauncherState("no_vault_open");
  }

  function clearActiveNote() {
    if (!confirmDiscardUnsaved()) {
      return;
    }

    setActiveNote(null);
    setNoteBody("");
    setSavedNoteBody("");
    setNoteRevision(null);
    setSaveState("idle");
    setSaveError(null);
    setFileOperationError(null);
    setContentError(null);
    setContentLoading(false);
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
          onClearNote={clearActiveNote}
          onCreateNote={(parentRelPath) => {
            void handleCreateNote(parentRelPath);
          }}
          onCreateFolder={(parentRelPath) => {
            void handleCreateFolder(parentRelPath);
          }}
          onSaveNote={() => {
            void handleSaveNote();
          }}
          onRenameNote={(relPath) => {
            void handleRenameNote(relPath);
          }}
          onDeleteNote={(relPath) => {
            void handleDeleteNote(relPath);
          }}
          onNoteBodyChange={(value) => {
            setNoteBody(value);
            if (saveState !== "saving") {
              setSaveState("idle");
              setSaveError(null);
            }
          }}
          noteDirty={noteDirty}
          saveState={saveState}
          saveError={saveError}
          fileOperationError={fileOperationError}
          onSearchQueryChange={setSearchQueryValue}
        />
      )}

      {textDialog ? (
        <TextInputDialog
          title={textDialog.title}
          message={textDialog.message}
          defaultValue={textDialog.defaultValue}
          confirmLabel={textDialog.confirmLabel}
          onCancel={() => closeTextDialog(null)}
          onSubmit={(value) => closeTextDialog(value)}
        />
      ) : null}
    </div>
  );
}
