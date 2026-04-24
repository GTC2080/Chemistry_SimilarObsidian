import type { NoteInfo } from "../../types";
import { useT } from "../../i18n";

interface AIContextPanelProps {
  relatedNotes: NoteInfo[];
  resonanceLoading: boolean;
  onSelectNote: (note: NoteInfo) => void;
  contextOpen: boolean;
  onToggleContext: () => void;
}

export default function AIContextPanel({
  relatedNotes,
  resonanceLoading,
  onSelectNote,
  contextOpen,
  onToggleContext,
}: AIContextPanelProps) {
  const t = useT();

  return (
    <div className="shrink-0" style={{ borderTop: "0.5px solid var(--panel-border)" }}>
      <button
        type="button"
        onClick={onToggleContext}
        className="w-full px-3.5 py-2 flex items-center justify-between cursor-pointer hover:bg-[var(--sidebar-hover)] transition-colors duration-150"
      >
        <div className="flex items-center gap-2">
          <div className="relative">
            <svg
              className="w-3 h-3"
              style={{ color: "var(--text-quaternary)" }}
              viewBox="0 0 24 24"
              fill="none"
              stroke="currentColor"
              strokeWidth="2"
              strokeLinecap="round"
              strokeLinejoin="round"
            >
              <circle cx="12" cy="12" r="3" />
              <path d="M12 1v2M12 21v2M4.22 4.22l1.42 1.42M18.36 18.36l1.42 1.42M1 12h2M21 12h2M4.22 19.78l1.42-1.42M18.36 5.64l1.42-1.42" />
            </svg>
            {resonanceLoading && (
              <span
                className="absolute -top-0.5 -right-0.5 w-[4px] h-[4px] rounded-full animate-breathe"
                style={{ background: "var(--accent)" }}
              />
            )}
          </div>
          <span className="text-[11px] font-medium" style={{ color: "var(--text-tertiary)" }}>
            {t("ai.resonanceContext")}
          </span>
          {relatedNotes.length > 0 && (
            <span
              className="text-[10px] px-1.5 py-0.5 rounded-md"
              style={{ background: "rgba(10,132,255,0.1)", color: "var(--accent)" }}
            >
              {relatedNotes.length}
            </span>
          )}
        </div>
        <svg
          className="w-3 h-3 transition-transform duration-200"
          style={{
            color: "var(--text-quaternary)",
            transform: contextOpen ? "rotate(0deg)" : "rotate(-90deg)",
          }}
          viewBox="0 0 24 24"
          fill="none"
          stroke="currentColor"
          strokeWidth="2"
          strokeLinecap="round"
          strokeLinejoin="round"
        >
          <polyline points="6 9 12 15 18 9" />
        </svg>
      </button>

      {contextOpen && relatedNotes.length > 0 && (
        <div className="px-2.5 pb-2 flex flex-wrap gap-1.5 max-h-[18vh] overflow-y-auto">
          {relatedNotes.map((note, index) => (
            <button
              type="button"
              key={note.id}
              onClick={() => onSelectNote(note)}
              className="animate-fade-in inline-flex items-center gap-1.5 px-2.5 py-[5px] rounded-[8px] text-[11px] cursor-pointer transition-all duration-150 hover:bg-[var(--sidebar-hover)] active:scale-[0.97]"
              title={note.name}
              style={{
                animationDelay: `${index * 30}ms`,
                background: "var(--subtle-surface)",
                color: "var(--text-secondary)",
              }}
            >
              <svg
                className="w-3 h-3 shrink-0"
                style={{ color: "var(--text-quaternary)" }}
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
              <span className="truncate max-w-[120px]">{note.name}</span>
            </button>
          ))}
        </div>
      )}
    </div>
  );
}
