import { lazy, Suspense, useMemo, useState } from "react";
import type { FileCategory, MolecularPreviewMeta, NoteInfo } from "../../types";
import type { RuntimeSettings } from "../settings/settingsTypes";
import ChemEditorLoading from "../chem-editor/ChemEditorLoading";
import LaunchSplash from "./LaunchSplash";

const MarkdownEditor = lazy(() => import("../MarkdownEditor"));
const PublishStudio = lazy(() => import("../publish-studio"));
const SpectroscopyViewer = lazy(() => import("../SpectroscopyViewer"));
const MolecularViewer3D = lazy(() => import("../MolecularViewer3D"));
const SymmetryViewer3D = lazy(() => import("../SymmetryViewer3D"));
const CrystalViewer3D = lazy(() => import("../CrystalViewer3D"));
const ChemDrawBoard = lazy(() => import("../chem-editor/ChemDrawBoard"));
const MediaViewer = lazy(() =>
  import("../media-viewer").then(module => ({ default: module.MediaViewer }))
);
const PdfViewer = lazy(() => import("../pdf-viewer/PdfViewer"));

interface ActiveNoteContentProps {
  vaultPath: string;
  notes: NoteInfo[];
  activeNote: NoteInfo;
  activeCategory: FileCategory | null;
  noteContent: string;
  molecularPreview: MolecularPreviewMeta | null;
  binaryPreviewUrl: string;
  runtimeSettings: RuntimeSettings;
  onSave: (markdown: string) => void | Promise<void>;
  onLiveContentChange: (content: string) => void;
}

type MolecularViewMode = "structure" | "symmetry" | "crystal";

function PlainTextContent({ noteContent }: { noteContent: string }) {
  return (
    <div className="flex-1 overflow-auto">
      <pre
        className="px-10 py-6 text-[13px] leading-relaxed whitespace-pre-wrap break-words
          text-[var(--text-secondary)] font-mono"
      >
        <code>{noteContent}</code>
      </pre>
    </div>
  );
}

function MolecularContent({
  activeNote,
  noteContent,
  molecularPreview,
  runtimeSettings,
}: Pick<ActiveNoteContentProps, "activeNote" | "noteContent" | "molecularPreview" | "runtimeSettings">) {
  const [molecularViewMode, setMolecularViewMode] = useState<MolecularViewMode>("structure");

  if (runtimeSettings.activeDiscipline !== "chemistry") {
    return <PlainTextContent noteContent={noteContent} />;
  }

  const isCif = activeNote.file_extension.toLowerCase() === "cif";

  const tabs: { key: MolecularViewMode; label: string }[] = [
    { key: "structure", label: "结构" },
    { key: "symmetry", label: "对称性" },
    ...(isCif ? [{ key: "crystal" as const, label: "晶格" }] : []),
  ];

  return (
    <div className="flex-1 flex flex-col min-h-0">
      <div
        className="flex items-center gap-0 px-4 py-1.5 border-b-[0.5px] border-b-[var(--panel-border)]"
        style={{ background: "var(--subtle-surface)" }}
      >
        {tabs.map((tab, i) => (
          <button
            key={tab.key}
            type="button"
            onClick={() => setMolecularViewMode(tab.key)}
            className={`px-3 py-1 text-[11px] font-medium cursor-pointer transition-colors border ${
              i === 0 ? "rounded-l-md" : ""
            } ${i === tabs.length - 1 ? "rounded-r-md" : ""} ${
              i > 0 ? "border-l-0" : ""
            } ${
              molecularViewMode === tab.key
                ? "bg-[var(--accent)] border-[var(--accent)] text-white"
                : "bg-transparent border-[var(--panel-border)] text-[var(--text-quaternary)] hover:text-[var(--text-tertiary)]"
            }`}
          >
            {tab.label}
          </button>
        ))}
      </div>

      {molecularViewMode === "structure" && (
        <MolecularViewer3D
          key={`struct-${activeNote.id}`}
          data={noteContent}
          format={activeNote.file_extension}
          filePath={activeNote.path}
          previewMeta={molecularPreview}
        />
      )}
      {molecularViewMode === "symmetry" && (
        <SymmetryViewer3D
          key={`sym-${activeNote.id}`}
          data={noteContent}
          format={activeNote.file_extension}
          filePath={activeNote.path}
        />
      )}
      {molecularViewMode === "crystal" && isCif && (
        <CrystalViewer3D
          key={`crystal-${activeNote.id}`}
          data={noteContent}
          format={activeNote.file_extension}
          filePath={activeNote.path}
        />
      )}
    </div>
  );
}

export default function ActiveNoteContent({
  vaultPath,
  notes,
  activeNote,
  activeCategory,
  noteContent,
  molecularPreview,
  binaryPreviewUrl,
  runtimeSettings,
  onSave,
  onLiveContentChange,
}: ActiveNoteContentProps) {
  const renderedContent = useMemo(() => {
    switch (activeCategory) {
      case "markdown":
        return (
          <MarkdownEditor
            key={activeNote.id}
            initialContent={noteContent}
            onSave={onSave}
            onContentChange={onLiveContentChange}
            vaultPath={vaultPath}
            fontFamily={runtimeSettings.fontFamily}
            enableScientific={
              runtimeSettings.enableScientific || runtimeSettings.activeDiscipline === "chemistry"
            }
            activeDiscipline={runtimeSettings.activeDiscipline}
          />
        );
      case "chem":
        return (
          <ChemDrawBoard
            key={activeNote.id}
            initialContent={noteContent}
            onSave={onSave}
          />
        );
      case "paper":
        return (
          <PublishStudio
            key={activeNote.id}
            notes={notes}
            initialContent={noteContent}
            onSave={onSave}
          />
        );
      case "spectroscopy":
        return <SpectroscopyViewer key={activeNote.id} note={activeNote} />;
      case "molecular":
        return (
          <MolecularContent
            activeNote={activeNote}
            noteContent={noteContent}
            molecularPreview={molecularPreview}
            runtimeSettings={runtimeSettings}
          />
        );
      case "image":
        return <MediaViewer category="image" note={activeNote} binaryPreviewUrl={binaryPreviewUrl} />;
      case "pdf":
        return <PdfViewer key={activeNote.id} note={activeNote} />;
      default:
        return <PlainTextContent noteContent={noteContent} />;
    }
  }, [
    activeCategory,
    activeNote,
    binaryPreviewUrl,
    molecularPreview,
    noteContent,
    notes,
    onLiveContentChange,
    onSave,
    runtimeSettings,
    vaultPath,
  ]);

  const fallback = activeCategory === "chem"
    ? <ChemEditorLoading />
    : <LaunchSplash />;

  return <Suspense fallback={fallback}>{renderedContent}</Suspense>;
}
