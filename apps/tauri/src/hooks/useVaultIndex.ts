import { useCallback, useEffect, useRef, useState } from "react";
import { invoke } from "@tauri-apps/api/core";
import type { NoteInfo } from "../types";
import { useFileWatcher } from "./useFileWatcher";

interface UseVaultIndexOptions {
  vaultPath: string;
  ignoredFolders: string;
  activeNote: NoteInfo | null;
  onActiveNoteMissing: () => void;
}

/** Phase 1: fast metadata-only walk — returns file list immediately. */
async function scanVaultNotes(vaultPath: string, ignoredFolders: string) {
  return invoke<NoteInfo[]>("scan_vault", {
    vaultPath,
    ignoredFolders: ignoredFolders || "",
  });
}

/** Phase 2: kernel-backed Markdown cache refresh + embeddings. */
function startBackgroundIndex(vaultPath: string, ignoredFolders: string) {
  invoke<number>("index_vault_content", {
    vaultPath,
    ignoredFolders: ignoredFolders || "",
  }).catch((err) => {
    console.warn("[index_vault_content]", err);
  });
}

export function useVaultIndex({
  vaultPath,
  ignoredFolders,
  activeNote,
  onActiveNoteMissing,
}: UseVaultIndexOptions) {
  const [notes, setNotes] = useState<NoteInfo[]>([]);
  const activeNoteRef = useRef<NoteInfo | null>(activeNote);

  useEffect(() => {
    activeNoteRef.current = activeNote;
  }, [activeNote]);

  const refreshNotes = useCallback(async (targetVaultPath = vaultPath) => {
    if (!targetVaultPath) {
      setNotes([]);
      return [];
    }

    // Phase 1: fast metadata scan — file tree appears immediately
    const refreshed = await scanVaultNotes(targetVaultPath, ignoredFolders);
    setNotes(refreshed);

    const currentActiveNote = activeNoteRef.current;
    if (currentActiveNote && !refreshed.some(note => note.id === currentActiveNote.id)) {
      onActiveNoteMissing();
    }

    // Phase 2: kernel-backed cache refresh (fire-and-forget)
    startBackgroundIndex(targetVaultPath, ignoredFolders);

    return refreshed;
  }, [ignoredFolders, onActiveNoteMissing, vaultPath]);

  // vaultPath 被清空时立即清空列表；所有扫描都通过 refreshNotes() 显式触发，
  // 避免 setVaultPath + refreshNotes 同时触发 useEffect 导致的双扫描。
  useEffect(() => {
    if (!vaultPath) {
      setNotes([]);
    }
  }, [vaultPath]);

  // 增量文件监听：外部工具修改文件后自动更新 UI，无需全盘扫描
  useFileWatcher({ vaultPath, ignoredFolders, setNotes });

  return {
    notes,
    setNotes,
    refreshNotes,
  };
}
