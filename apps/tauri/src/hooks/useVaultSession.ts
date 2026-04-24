import { useCallback, useEffect, useRef, useState } from "react";
import { invoke } from "@tauri-apps/api/core";
import { open } from "@tauri-apps/plugin-dialog";
import type { DisciplineProfile } from "../components/settings/settingsTypes";
import type { MolecularPreviewMeta, NoteInfo } from "../types";
import { perf } from "../utils/perf";
import { useActiveNoteContent } from "./useActiveNoteContent";
import { useNotePersistence } from "./useNotePersistence";
import { useVaultIndex } from "./useVaultIndex";

interface UseVaultSessionOptions {
  ignoredFolders: string;
  activeDiscipline: DisciplineProfile;
  onSaveToRecent: (path: string) => Promise<void>;
}

export function useVaultSession({
  ignoredFolders,
  activeDiscipline,
  onSaveToRecent,
}: UseVaultSessionOptions) {
  const [vaultPath, setVaultPath] = useState("");
  const [activeNote, setActiveNote] = useState<NoteInfo | null>(null);
  const [error, setError] = useState("");
  const [scanning, setScanning] = useState(false);
  // Track whether the initial vault open has completed (to distinguish
  // "first open" from "ignoredFolders changed while vault is open").
  const vaultReadyRef = useRef(false);

  const clearActiveSelection = useCallback(() => {
    setActiveNote(null);
  }, []);

  const {
    notes,
    setNotes,
    refreshNotes,
  } = useVaultIndex({
    vaultPath,
    ignoredFolders,
    activeNote,
    onActiveNoteMissing: clearActiveSelection,
  });

  const {
    noteContent,
    setNoteContent,
    liveContent,
    setLiveContent,
    molecularPreview,
    binaryPreviewUrl,
    resetContent,
  } = useActiveNoteContent({
    activeNote,
    activeDiscipline,
  });

  const { enqueueSave, flushPendingSave } = useNotePersistence({
    onError: setError,
  });

  const openVaultByPath = useCallback(async (path: string) => {
    try {
      await flushPendingSave();
      setError("");
      setScanning(true);
      vaultReadyRef.current = false;
      setVaultPath(path);
      setActiveNote(null);
      resetContent();

      const endVaultOpen = perf.start("vault-open");
      try {
        await invoke("init_vault", { vaultPath: path });
        await refreshNotes(path);
        endVaultOpen();
        vaultReadyRef.current = true;
        await onSaveToRecent(path);
      } catch (cause) {
        setError(cause instanceof Error ? cause.message : String(cause));
      } finally {
        setScanning(false);
      }
    } catch (cause) {
      setError(cause instanceof Error ? cause.message : String(cause));
      setScanning(false);
    }
  }, [flushPendingSave, onSaveToRecent, refreshNotes, resetContent]);

  // When ignoredFolders changes while a vault is already open, rescan.
  const prevIgnoredRef = useRef(ignoredFolders);
  useEffect(() => {
    if (prevIgnoredRef.current === ignoredFolders) return;
    prevIgnoredRef.current = ignoredFolders;
    if (!vaultReadyRef.current || !vaultPath) return;
    // Vault is open and ignoredFolders changed — rescan.
    setScanning(true);
    refreshNotes(vaultPath).finally(() => setScanning(false));
  }, [ignoredFolders, refreshNotes, vaultPath]);

  const handleOpenVault = useCallback(async () => {
    try {
      setError("");
      const selected = await open({ directory: true, multiple: false });
      if (!selected) {
        return;
      }
      await openVaultByPath(selected);
    } catch (cause) {
      setError(cause instanceof Error ? cause.message : String(cause));
    }
  }, [openVaultByPath]);

  const handleSelectNote = useCallback((note: NoteInfo) => {
    // Fire save flush in background — don't block note switch on save.
    flushPendingSave().catch((cause) => {
      setError(cause instanceof Error ? cause.message : String(cause));
    });
    setError("");
    setActiveNote(note);
  }, [flushPendingSave]);

  const handleBackToManager = useCallback(async () => {
    await flushPendingSave();
    setVaultPath("");
    setNotes([]);
    setActiveNote(null);
    resetContent();
    setError("");
  }, [flushPendingSave, resetContent, setNotes]);

  const handleSave = useCallback(async (markdown: string) => {
    if (!activeNote || !vaultPath) {
      return;
    }
    enqueueSave(vaultPath, activeNote.path, markdown);
  }, [activeNote, enqueueSave, vaultPath]);

  return {
    vaultPath,
    notes,
    activeNote,
    noteContent,
    liveContent,
    molecularPreview: molecularPreview as MolecularPreviewMeta | null,
    binaryPreviewUrl,
    error,
    loading: scanning,
    setNotes,
    setActiveNote,
    setNoteContent,
    setLiveContent,
    setError,
    openVaultByPath,
    handleOpenVault,
    handleSelectNote,
    handleBackToManager,
    handleSave,
    flushPendingSave,
  };
}
