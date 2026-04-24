import { lazy, Suspense } from "react";
import type { NoteInfo } from "../../types";
import type { RuntimeSettings } from "../settings/settingsTypes";
import type { TruthState } from "../../models/truth_system";

const SemanticSearchModal = lazy(() =>
  import("../search").then(module => ({ default: module.SemanticSearchModal }))
);
const GlobalGraphModal = lazy(() =>
  import("../global-graph").then(module => ({ default: module.GlobalGraphModal }))
);
const SettingsModal = lazy(() => import("../SettingsModal"));
const TruthDashboard = lazy(() => import("../TruthDashboard"));

interface AppModalsProps {
  searchModalReady: boolean;
  searchOpen: boolean;
  graphModalReady: boolean;
  graphOpen: boolean;
  settingsModalReady: boolean;
  settingsOpen: boolean;
  truthReady: boolean;
  truthOpen: boolean;
  notes: NoteInfo[];
  truthState: TruthState;
  onCloseSearch: () => void;
  onCloseGraph: () => void;
  onCloseSettings: () => void;
  onCloseTruth: () => void;
  onSelectNote: (note: NoteInfo) => void | Promise<void>;
  onSettingsApplied: (settings: RuntimeSettings) => void;
}

export default function AppModals({
  searchModalReady,
  searchOpen,
  graphModalReady,
  graphOpen,
  settingsModalReady,
  settingsOpen,
  truthReady,
  truthOpen,
  notes,
  truthState,
  onCloseSearch,
  onCloseGraph,
  onCloseSettings,
  onCloseTruth,
  onSelectNote,
  onSettingsApplied,
}: AppModalsProps) {
  return (
    <>
      {searchModalReady && (
        <Suspense fallback={null}>
          <SemanticSearchModal
            open={searchOpen}
            onClose={onCloseSearch}
            onSelect={note => { void onSelectNote(note); }}
          />
        </Suspense>
      )}
      {graphModalReady && (
        <Suspense fallback={null}>
          <GlobalGraphModal
            open={graphOpen}
            onClose={onCloseGraph}
            onNavigate={note => { void onSelectNote(note); }}
            notes={notes}
          />
        </Suspense>
      )}
      {settingsModalReady && (
        <Suspense fallback={null}>
          <SettingsModal
            open={settingsOpen}
            onClose={onCloseSettings}
            onSettingsApplied={onSettingsApplied}
          />
        </Suspense>
      )}
      {truthReady && (
        <Suspense fallback={null}>
          <TruthDashboard
            open={truthOpen}
            onClose={onCloseTruth}
            state={truthState}
          />
        </Suspense>
      )}
    </>
  );
}
