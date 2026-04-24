import type { NoteInfo } from "../../types";
import { useT } from "../../i18n";
import { TagTreeItem, type TagTreeNode } from "./TagTree";

interface SidebarTagsPanelProps {
  tagsCount: number;
  tagTree: TagTreeNode[];
  selectedTag: string | null;
  tagNotes: NoteInfo[];
  tagNotesLoading: boolean;
  activeNoteId: string | null;
  onSelectTag: (tag: string) => void;
  onSelectNote: (note: NoteInfo) => void;
}

export default function SidebarTagsPanel({
  tagsCount,
  tagTree,
  selectedTag,
  tagNotes,
  tagNotesLoading,
  activeNoteId,
  onSelectTag,
  onSelectNote,
}: SidebarTagsPanelProps) {
  const t = useT();
  return (
    <>
      {tagsCount === 0 && (
        <div className="flex flex-col items-center py-16 gap-3">
          <div className="w-11 h-11 rounded-[13px] flex items-center justify-center" style={{ background: "var(--subtle-surface)" }}>
            <svg
              className="w-5 h-5"
              style={{ color: "var(--text-quaternary)" }}
              viewBox="0 0 24 24"
              fill="none"
              stroke="currentColor"
              strokeWidth="1.5"
              strokeLinecap="round"
              strokeLinejoin="round"
            >
              <path d="M20.59 13.41l-7.17 7.17a2 2 0 0 1-2.83 0L2 12V2h10l8.59 8.59a2 2 0 0 1 0 2.82z" />
              <line x1="7" y1="7" x2="7.01" y2="7" />
            </svg>
          </div>
          <p className="text-[12px] text-center leading-relaxed max-w-[180px]" style={{ color: "var(--text-quaternary)" }}>
            {t("sidebar.tagHint")}
            <br />
            {t("sidebar.tagHint2")}
          </p>
        </div>
      )}
      {tagTree.map(node => (
        <TagTreeItem
          key={node.fullPath}
          node={node}
          depth={0}
          onSelectTag={onSelectTag}
          selectedTag={selectedTag}
        />
      ))}
      {selectedTag && (
        <div className="mt-3 pt-3" style={{ borderTop: "0.5px solid var(--separator-light)" }}>
          <div className="px-2.5 pb-2 flex items-center gap-1.5">
            <span className="text-[11px] font-medium" style={{ color: "var(--text-quaternary)" }}>
              #{selectedTag}
            </span>
            {!tagNotesLoading && (
              <span className="text-[10px]" style={{ color: "var(--text-quinary)" }}>{tagNotes.length}</span>
            )}
          </div>
          {tagNotesLoading && (
            <div className="flex items-center justify-center py-4">
              <div
                className="w-4 h-4 rounded-full border-[1.5px] animate-spin"
                style={{ borderColor: "rgba(255,255,255,0.06)", borderTopColor: "var(--accent)" }}
              />
            </div>
          )}
          {!tagNotesLoading && tagNotes.map(note => {
            const isActive = activeNoteId === note.id;
            return (
              <button
                key={note.id}
                onClick={() => onSelectNote(note)}
                className="w-full text-left px-3 py-[6px] rounded-[10px] text-[12px] transition-colors duration-150 cursor-pointer flex items-center gap-2 relative hover:bg-[var(--sidebar-hover)]"
                style={{ background: isActive ? "rgba(10,132,255,0.12)" : "transparent" }}
              >
                {isActive && (
                  <div
                    className="absolute left-[3px] top-1/2 -translate-y-1/2 w-[3px] h-[12px] rounded-full"
                    style={{ background: "var(--accent)" }}
                  />
                )}
                <svg
                  className="w-3.5 h-3.5 shrink-0"
                  style={{ color: isActive ? "var(--text-secondary)" : "var(--text-quaternary)" }}
                  viewBox="0 0 24 24"
                  fill="none"
                  stroke="currentColor"
                  strokeWidth="1.5"
                  strokeLinecap="round"
                  strokeLinejoin="round"
                >
                  <path d="M14 2H6a2 2 0 0 0-2 2v16a2 2 0 0 0 2 2h12a2 2 0 0 0 2-2V8z" />
                  <polyline points="14 2 14 8 20 8" />
                </svg>
                <span
                  className="truncate"
                  style={{
                    color: isActive ? "var(--text-primary)" : "var(--text-secondary)",
                    fontWeight: isActive ? 500 : 400,
                  }}
                >
                  {note.name}
                </span>
              </button>
            );
          })}
        </div>
      )}
    </>
  );
}
