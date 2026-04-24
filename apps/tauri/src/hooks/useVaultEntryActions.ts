import { useCallback, type Dispatch, type SetStateAction } from "react";
import { invoke } from "@tauri-apps/api/core";
import type { NoteInfo } from "../types";

interface UseVaultEntryActionsParams {
  vaultPath: string;
  ignoredFolders: string;
  activeNote: NoteInfo | null;
  setNotes: Dispatch<SetStateAction<NoteInfo[]>>;
  setActiveNote: Dispatch<SetStateAction<NoteInfo | null>>;
  setNoteContent: Dispatch<SetStateAction<string>>;
  setLiveContent: Dispatch<SetStateAction<string>>;
  setError: Dispatch<SetStateAction<string>>;
  onSelectNote: (note: NoteInfo) => void | Promise<void>;
}

function normalizeVaultPath(vaultPath: string): string {
  return vaultPath.replace(/[\\/]+$/, "");
}

function normalizeRelativePath(relativePath: string): string {
  return relativePath.replace(/\\/g, "/").replace(/^\/+|\/+$/g, "");
}

export function useVaultEntryActions({
  vaultPath,
  ignoredFolders,
  activeNote,
  setNotes,
  setActiveNote,
  setNoteContent,
  setLiveContent,
  setError,
  onSelectNote,
}: UseVaultEntryActionsParams) {
  const refreshNotes = useCallback(async () => {
    const updated = await invoke<NoteInfo[]>("scan_vault", {
      vaultPath,
      ignoredFolders: ignoredFolders || "",
    });
    setNotes(updated);
    return updated;
  }, [vaultPath, ignoredFolders, setNotes]);

  const handleCreateFile = useCallback(async (kind: "note" | "mol" | "paper", targetFolderRelativePath = "") => {
    if (!vaultPath) return;
    const stamp = new Date().toISOString().replace(/[:.]/g, "-");
    const extension = kind === "mol"
      ? "mol"
      : kind === "paper"
        ? "paper"
        : "md";
    const baseName = kind === "mol"
      ? "未命名分子"
      : kind === "paper"
        ? "未命名论文"
        : "未命名笔记";
    const fileName = `${baseName} ${stamp}.${extension}`;
    const normalizedVault = normalizeVaultPath(vaultPath);
    const normalizedFolder = normalizeRelativePath(targetFolderRelativePath);
    const folderPath = normalizedFolder ? `${normalizedVault}/${normalizedFolder}` : normalizedVault;
    const filePath = `${folderPath}/${fileName}`;
    const initial = kind === "mol"
      ? ""
      : kind === "paper"
        ? JSON.stringify(
          {
            nodeIds: [],
            template: "standard-thesis",
            cslPath: "",
            bibliographyPath: "",
          },
          null,
          2
        )
        : "# 未命名\n";

    try {
      setError("");
      await invoke("write_note", { vaultPath, filePath, content: initial });
      const updated = await refreshNotes();
      const created = updated.find(note => note.path === filePath || note.id.endsWith(fileName));
      if (created) {
        await onSelectNote(created);
      }
    } catch (e) {
      setError(`新建失败: ${e instanceof Error ? e.message : String(e)}`);
    }
  }, [vaultPath, refreshNotes, setError, onSelectNote]);

  const handleDeleteEntry = useCallback(async (absolutePath: string, targetLabel: string, isFolder: boolean) => {
    if (!vaultPath) return;
    const ok = window.confirm(
      `确认删除${isFolder ? "文件夹" : "文件"}「${targetLabel}」？\n此操作不可恢复。`
    );
    if (!ok) return;

    try {
      setError("");

      // 乐观更新：先从本地状态移除，让 UI 立即响应
      const normalizedTarget = absolutePath.replace(/\\/g, "/");
      setNotes(prev => prev.filter(n => {
        const normalizedPath = n.path.replace(/\\/g, "/");
        return normalizedPath !== normalizedTarget && !(isFolder && normalizedPath.startsWith(`${normalizedTarget}/`));
      }));

      if (activeNote) {
        const normalizedActive = activeNote.path.replace(/\\/g, "/");
        const deletedCurrent = normalizedActive === normalizedTarget;
        const deletedUnderFolder = isFolder && normalizedActive.startsWith(`${normalizedTarget}/`);
        if (deletedCurrent || deletedUnderFolder) {
          setActiveNote(null);
          setNoteContent("");
          setLiveContent("");
        }
      }

      await invoke("delete_entry", { vaultPath, targetPath: absolutePath });
      await refreshNotes();
    } catch (e) {
      // 失败时刷新以恢复真实状态
      await refreshNotes().catch(() => {});
      setError(`删除失败: ${e instanceof Error ? e.message : String(e)}`);
    }
  }, [
    vaultPath,
    activeNote,
    refreshNotes,
    setError,
    setActiveNote,
    setNoteContent,
    setLiveContent,
  ]);

  const handleMoveEntry = useCallback(async (sourceRelativePath: string, destFolderRelativePath: string) => {
    if (!vaultPath) return;
    const normalizedVault = normalizeVaultPath(vaultPath);
    const sourcePath = `${normalizedVault}/${sourceRelativePath}`;
    const destFolder = destFolderRelativePath
      ? `${normalizedVault}/${destFolderRelativePath}`
      : normalizedVault;

    try {
      setError("");
      await invoke("move_entry", { vaultPath, sourcePath, destFolder });
      const updated = await refreshNotes();

      if (activeNote) {
        const oldId = activeNote.id.replace(/\\/g, "/");
        const sourceName = sourceRelativePath.split("/").pop() || "";
        const newPrefix = destFolderRelativePath ? `${destFolderRelativePath}/` : "";
        const newId = `${newPrefix}${sourceName}`;
        if (oldId === sourceRelativePath || oldId.startsWith(sourceRelativePath + "/")) {
          const suffix = oldId.substring(sourceRelativePath.length);
          const updatedId = `${newId}${suffix}`;
          const found = updated.find(n => n.id.replace(/\\/g, "/") === updatedId);
          if (found) await onSelectNote(found);
        }
      }
    } catch (e) {
      setError(`移动失败: ${e instanceof Error ? e.message : String(e)}`);
    }
  }, [vaultPath, activeNote, refreshNotes, setError, onSelectNote]);

  const handleCreateFolder = useCallback(async (targetParentRelativePath = "") => {
    if (!vaultPath) return;
    const folderName = window.prompt("新建文件夹名称：");
    if (folderName == null) return;
    const trimmed = folderName.trim();
    if (!trimmed) return;
    if (trimmed.includes("/") || trimmed.includes("\\")) {
      setError("新建失败: 文件夹名称不能包含 / 或 \\");
      return;
    }

    const normalizedVault = normalizeVaultPath(vaultPath);
    const normalizedParent = normalizeRelativePath(targetParentRelativePath);
    const folderPath = normalizedParent
      ? `${normalizedVault}/${normalizedParent}/${trimmed}`
      : `${normalizedVault}/${trimmed}`;

    try {
      setError("");
      await invoke("create_folder", { vaultPath, folderPath });
      await refreshNotes();
    } catch (e) {
      setError(`新建文件夹失败: ${e instanceof Error ? e.message : String(e)}`);
    }
  }, [vaultPath, setError, refreshNotes]);

  const handleRenameEntryInline = useCallback(async (
    sourceRelativePath: string,
    newName: string
  ) => {
    if (!vaultPath) return;
    const trimmed = newName.trim();
    if (!trimmed) return;
    if (trimmed.includes("/") || trimmed.includes("\\")) {
      setError("重命名失败: 名称不能包含 / 或 \\");
      return;
    }

    const normalizedVault = normalizeVaultPath(vaultPath);
    const sourcePath = `${normalizedVault}/${sourceRelativePath}`;

    try {
      setError("");
      await invoke("rename_entry", { vaultPath, sourcePath, newName: trimmed });
      const updated = await refreshNotes();

      if (activeNote) {
        const oldId = activeNote.id.replace(/\\/g, "/");
        const sourceParent = sourceRelativePath.includes("/")
          ? sourceRelativePath.substring(0, sourceRelativePath.lastIndexOf("/"))
          : "";
        const newRelative = sourceParent ? `${sourceParent}/${trimmed}` : trimmed;
        if (oldId === sourceRelativePath || oldId.startsWith(sourceRelativePath + "/")) {
          const suffix = oldId.substring(sourceRelativePath.length);
          const updatedId = `${newRelative}${suffix}`;
          const found = updated.find(n => n.id.replace(/\\/g, "/") === updatedId);
          if (found) await onSelectNote(found);
        }
      }
    } catch (e) {
      setError(`重命名失败: ${e instanceof Error ? e.message : String(e)}`);
    }
  }, [vaultPath, activeNote, refreshNotes, setError, onSelectNote]);

  const handleRenameEntry = useCallback(async (
    sourceRelativePath: string,
    currentFullName: string,
    isFolder: boolean
  ) => {
    if (!vaultPath) return;
    const nextName = window.prompt(`重命名${isFolder ? "文件夹" : "文件"}：`, currentFullName);
    if (nextName == null) return;
    const trimmed = nextName.trim();
    if (!trimmed || trimmed === currentFullName) return;
    await handleRenameEntryInline(sourceRelativePath, trimmed);
  }, [vaultPath, handleRenameEntryInline]);

  return {
    handleCreateFile,
    handleDeleteEntry,
    handleMoveEntry,
    handleCreateFolder,
    handleRenameEntryInline,
    handleRenameEntry,
  };
}
