import { lazy, Suspense, useEffect, useMemo, useRef, useState } from "react";
import { useVaultEntryActions } from "../../hooks/useVaultEntryActions";
import { useTruthSystem } from "../../hooks/useTruthSystem";
import { useLazyModalReady } from "../../hooks/useLazyModalReady";
import { useAppShortcuts } from "../../hooks/useAppShortcuts";
import { useStudyTracker } from "../../hooks/useStudyTracker";
import { useVaultSession } from "../../hooks/useVaultSession";
import { getFileCategory } from "../../types";
import type { TruthState } from "../../models/truth_system";
import type { RuntimeSettings } from "../settings/settingsTypes";
import AppModals from "./AppModals";
import LaunchSplash from "./LaunchSplash";

const WorkspaceShell = lazy(() => import("./WorkspaceShell"));

interface WorkspaceRuntimeProps {
  initialVaultPath: string;
  runtimeSettings: RuntimeSettings;
  onRuntimeSettingsApplied: (settings: RuntimeSettings) => void;
  onSaveToRecent: (path: string) => Promise<void>;
  onExitWorkspace: () => void;
  onTruthStateSnapshot: (state: TruthState) => void;
}

export default function WorkspaceRuntime({
  initialVaultPath,
  runtimeSettings,
  onRuntimeSettingsApplied,
  onSaveToRecent,
  onExitWorkspace,
  onTruthStateSnapshot,
}: WorkspaceRuntimeProps) {
  const {
    vaultPath,
    notes,
    activeNote,
    noteContent,
    liveContent,
    molecularPreview,
    binaryPreviewUrl,
    error,
    loading,
    setNotes,
    setActiveNote,
    setNoteContent,
    setLiveContent,
    setError,
    openVaultByPath,
    handleSelectNote,
    handleBackToManager,
    handleSave,
  } = useVaultSession({
    ignoredFolders: runtimeSettings.ignoredFolders,
    activeDiscipline: runtimeSettings.activeDiscipline,
    onSaveToRecent,
  });

  useStudyTracker(activeNote, vaultPath);

  const [searchOpen, setSearchOpen] = useState(false);
  const [graphOpen, setGraphOpen] = useState(false);
  const [settingsOpen, setSettingsOpen] = useState(false);
  const [aiSidebarOpen, setAiSidebarOpen] = useState(true);
  const [truthOpen, setTruthOpen] = useState(false);
  const [kineticsOpen, setKineticsOpen] = useState(false);
  const [timelineOpen, setTimelineOpen] = useState(false);
  const openedRef = useRef<string | null>(null);

  useEffect(() => {
    if (!initialVaultPath) return;
    if (openedRef.current === initialVaultPath && vaultPath === initialVaultPath) return;
    openedRef.current = initialVaultPath;
    void openVaultByPath(initialVaultPath);
  }, [initialVaultPath, vaultPath, openVaultByPath]);

  const activeCategory = useMemo(
    () => (activeNote ? getFileCategory(activeNote.file_extension) : null),
    [activeNote]
  );
  const canOpenKinetics = runtimeSettings.activeDiscipline === "chemistry";

  useEffect(() => {
    if (!canOpenKinetics && kineticsOpen) {
      setKineticsOpen(false);
    }
  }, [canOpenKinetics, kineticsOpen]);

  useEffect(() => {
    setKineticsOpen(false);
    setTimelineOpen(false);
  }, [activeNote]);

  const { truthState } = useTruthSystem({
    active: !!vaultPath,
  });

  useEffect(() => {
    onTruthStateSnapshot(truthState);
  }, [truthState, onTruthStateSnapshot]);

  useAppShortcuts({
    vaultLoaded: !!vaultPath,
    onOpenSearch: () => setSearchOpen(true),
    onOpenGraph: () => setGraphOpen(true),
    onToggleAI: () => setAiSidebarOpen(prev => !prev),
    onOpenSettings: () => setSettingsOpen(true),
  });

  const searchModalReady = useLazyModalReady(searchOpen);
  const graphModalReady = useLazyModalReady(graphOpen);
  const settingsModalReady = useLazyModalReady(settingsOpen);
  const truthReady = useLazyModalReady(truthOpen);

  const {
    handleCreateFile,
    handleDeleteEntry,
    handleMoveEntry,
    handleCreateFolder,
    handleRenameEntryInline,
    handleRenameEntry,
  } = useVaultEntryActions({
    vaultPath,
    ignoredFolders: runtimeSettings.ignoredFolders,
    activeNote,
    setNotes,
    setActiveNote,
    setNoteContent,
    setLiveContent,
    setError,
    onSelectNote: handleSelectNote,
  });

  const handleCloseNote = () => {
    setActiveNote(null);
    setNoteContent("");
    setLiveContent("");
  };

  const exitToManager = async () => {
    await handleBackToManager();
    onExitWorkspace();
  };

  return (
    <>
      <Suspense fallback={<LaunchSplash />}>
        <WorkspaceShell
          vaultPath={vaultPath}
          notes={notes}
          activeNote={activeNote}
          loading={loading}
          error={error}
          activeCategory={activeCategory}
          noteContent={noteContent}
          liveContent={liveContent}
          molecularPreview={molecularPreview}
          binaryPreviewUrl={binaryPreviewUrl}
          runtimeSettings={runtimeSettings}
          aiSidebarOpen={aiSidebarOpen}
          truthLevel={truthState.level}
          canOpenKinetics={canOpenKinetics}
          kineticsOpen={kineticsOpen}
          timelineOpen={timelineOpen}
          onToggleTimeline={() => {
            const next = !timelineOpen;
            setTimelineOpen(next);
            if (next) setKineticsOpen(false);
          }}
          onCloseTimeline={() => setTimelineOpen(false)}
          onOpenSearch={() => setSearchOpen(true)}
          onOpenGraph={() => setGraphOpen(true)}
          onToggleAI={() => setAiSidebarOpen(prev => !prev)}
          onOpenKinetics={() => {
            const next = !kineticsOpen;
            setKineticsOpen(next);
            if (next) setTimelineOpen(false);
          }}
          onCloseKinetics={() => setKineticsOpen(false)}
          onCloseNote={handleCloseNote}
          onBackToManager={exitToManager}
          onOpenTruth={() => setTruthOpen(true)}
          onOpenSettings={() => setSettingsOpen(true)}
          onSelectNote={handleSelectNote}
          onSave={handleSave}
          onLiveContentChange={setLiveContent}
          onCreateFile={handleCreateFile}
          onDeleteEntry={handleDeleteEntry}
          onMoveEntry={handleMoveEntry}
          onRenameEntry={handleRenameEntry}
          onInlineRenameEntry={handleRenameEntryInline}
          onCreateFolder={handleCreateFolder}
        />
      </Suspense>

      <AppModals
        searchModalReady={searchModalReady}
        searchOpen={searchOpen}
        graphModalReady={graphModalReady}
        graphOpen={graphOpen}
        settingsModalReady={settingsModalReady}
        settingsOpen={settingsOpen}
        truthReady={truthReady}
        truthOpen={truthOpen}
        notes={notes}
        truthState={truthState}
        onCloseSearch={() => setSearchOpen(false)}
        onCloseGraph={() => setGraphOpen(false)}
        onCloseSettings={() => setSettingsOpen(false)}
        onCloseTruth={() => setTruthOpen(false)}
        onSelectNote={handleSelectNote}
        onSettingsApplied={onRuntimeSettingsApplied}
      />
    </>
  );
}
