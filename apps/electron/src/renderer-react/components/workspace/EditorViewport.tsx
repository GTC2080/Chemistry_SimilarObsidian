import { useT } from "../../i18n";
import type { NoteRecord } from "../../lib/files-tree";

interface EditorViewportProps {
  activeNote: NoteRecord | null;
  noteBody: string;
  contentLoading: boolean;
  contentError: string | null;
  noteDirty: boolean;
  saveState: "idle" | "saving" | "saved" | "error";
  saveError: string | null;
  fileOperationError: string | null;
  onCloseNote: () => void;
  onCreateNote: () => void;
  onSaveNote: () => void;
  onRenameNote: () => void;
  onDeleteNote: () => void;
  onNoteBodyChange: (value: string) => void;
}

export default function EditorViewport({
  activeNote,
  noteBody,
  contentLoading,
  contentError,
  noteDirty,
  saveState,
  saveError,
  fileOperationError,
  onCloseNote,
  onCreateNote,
  onSaveNote,
  onRenameNote,
  onDeleteNote,
  onNoteBodyChange
}: EditorViewportProps) {
  const t = useT();
  const liveTitle = deriveTitleFromBody(noteBody) || activeNote?.title || activeNote?.name || "未命名笔记";
  const saveLabel = saveState === "saving"
    ? "保存中"
    : saveState === "saved" && !noteDirty
      ? "已保存"
      : "保存";

  return (
    <main className="relative flex-1 flex flex-col min-w-0 workspace-panel m-0">
      {activeNote ? (
        <>
          <header
            className="mx-0 mt-0 px-6 py-2.5 flex items-center justify-between border-b-[0.5px] border-b-[var(--panel-border)]"
            style={{ background: "var(--subtle-surface)" }}
          >
            <div className="flex items-center gap-3 min-w-0">
              <div className="w-2 h-2 rounded-full shrink-0 bg-[var(--accent)] shadow-[0_0_8px_rgba(10,132,255,0.4)]" />
              <div className="min-w-0">
                <h1 className="text-[15px] font-semibold truncate tracking-[-0.01em] text-[var(--text-primary)]">
                  {liveTitle}
                </h1>
                <div className="text-[10px] mt-0.5 truncate text-[var(--text-quaternary)]">
                  {activeNote.relPath}
                </div>
              </div>
              <span className="text-[11px] px-2 py-0.5 rounded-lg shrink-0 bg-[var(--subtle-surface-strong)] text-[var(--text-quaternary)]">
                .{activeNote.fileExtension}
              </span>
            </div>
            <div className="flex items-center gap-2 shrink-0 ml-4">
              <span
                className="text-[11px] px-2 py-0.5 rounded-lg shrink-0"
                style={{
                  background: noteDirty ? "rgba(255,159,10,0.12)" : "var(--subtle-surface-strong)",
                  color: noteDirty ? "#ff9f0a" : "var(--text-quaternary)"
                }}
              >
                {noteDirty ? "未保存" : saveState === "saved" ? "已保存" : "同步"}
              </span>
              <span className="text-[11px] tabular-nums text-[var(--text-quaternary)]">
                {new Date(activeNote.updatedAtMs).toLocaleString("zh-CN")}
              </span>
              <button
                type="button"
                onClick={onSaveNote}
                disabled={!noteDirty || saveState === "saving"}
                className="px-3 h-7 rounded-[10px] text-[12px] font-medium transition-colors disabled:opacity-45 disabled:cursor-default"
                style={{
                  background: "var(--accent)",
                  color: "#fff",
                  boxShadow: "0 6px 18px rgba(10,132,255,0.18)"
                }}
                title="Ctrl+S"
              >
                {saveLabel}
              </button>
              <button
                type="button"
                onClick={onCreateNote}
                className="px-3 h-7 rounded-[10px] text-[12px] font-medium text-[var(--text-secondary)] bg-[var(--subtle-surface-strong)] hover:bg-[var(--sidebar-hover)] transition-colors"
                title="Ctrl+N"
              >
                新建
              </button>
              <button
                type="button"
                onClick={onRenameNote}
                className="px-3 h-7 rounded-[10px] text-[12px] font-medium text-[var(--text-secondary)] bg-[var(--subtle-surface-strong)] hover:bg-[var(--sidebar-hover)] transition-colors"
                title="重命名当前笔记"
              >
                重命名
              </button>
              <button
                type="button"
                onClick={onDeleteNote}
                className="px-3 h-7 rounded-[10px] text-[12px] font-medium text-[var(--text-secondary)] bg-[var(--subtle-surface-strong)] hover:text-[#ff453a] hover:bg-[rgba(255,69,58,0.08)] transition-colors"
                title="删除当前笔记"
              >
                删除
              </button>
              <button
                type="button"
                onClick={onCloseNote}
                className="w-6 h-6 rounded-md flex items-center justify-center text-[var(--text-quaternary)] hover:text-[var(--text-primary)] hover:bg-[var(--sidebar-hover)] transition-colors"
                title={t("viewport.closeDoc")}
              >
                <svg className="w-3.5 h-3.5" viewBox="0 0 24 24" fill="none" stroke="currentColor" strokeWidth="2" strokeLinecap="round" strokeLinejoin="round">
                  <line x1="18" y1="6" x2="6" y2="18" />
                  <line x1="6" y1="6" x2="18" y2="18" />
                </svg>
              </button>
            </div>
          </header>

          <div className="flex-1 overflow-y-auto px-8 py-6">
            {contentLoading ? (
              <div className="text-[13px] text-[var(--text-quaternary)]">正在加载内容…</div>
            ) : contentError ? (
              <div className="animate-fade-in px-4 py-2.5 rounded-xl text-[13px] bg-[rgba(255,69,58,0.08)] border-[0.5px] border-[rgba(255,69,58,0.12)] text-[#ff453a]">
                {contentError}
              </div>
            ) : (
              <div className="h-full min-h-[420px] flex flex-col gap-3">
                {saveError ? (
                  <div className="animate-fade-in px-4 py-2.5 rounded-xl text-[13px] bg-[rgba(255,69,58,0.08)] border-[0.5px] border-[rgba(255,69,58,0.12)] text-[#ff453a]">
                    {saveError}
                  </div>
                ) : null}
                {fileOperationError ? (
                  <div className="animate-fade-in px-4 py-2.5 rounded-xl text-[13px] bg-[rgba(255,159,10,0.08)] border-[0.5px] border-[rgba(255,159,10,0.15)] text-[#ff9f0a]">
                    {fileOperationError}
                  </div>
                ) : null}
                <textarea
                  value={noteBody}
                  onChange={(event) => onNoteBodyChange(event.target.value)}
                  className="note-editor-textarea flex-1"
                  spellCheck={false}
                />
              </div>
            )}
          </div>
        </>
      ) : (
        <div className="flex-1 flex items-center justify-center">
          <div className="text-center animate-fade-in">
            <svg
              className="mx-auto w-10 h-10 mb-5 text-[var(--text-quaternary)]"
              viewBox="0 0 24 24"
              fill="none"
              stroke="currentColor"
              strokeWidth="1.5"
              strokeLinecap="round"
              strokeLinejoin="round"
            >
              <path d="M14 2H6a2 2 0 0 0-2 2v16a2 2 0 0 0 2 2h12a2 2 0 0 0 2-2V8z" />
              <polyline points="14 2 14 8 20 8" />
              <line x1="16" y1="13" x2="8" y2="13" />
              <line x1="16" y1="17" x2="8" y2="17" />
            </svg>
            <p className="text-[14px] font-medium text-[var(--text-tertiary)]">{t("viewport.selectNote")}</p>
            <p className="text-[12px] mt-2 text-[var(--text-quaternary)]">
              {t("viewport.orSearch")} <kbd className="px-1.5 py-0.5 rounded text-[10px] font-mono bg-[rgba(118,118,128,0.12)] text-[var(--text-tertiary)]">Ctrl+K</kbd> {t("viewport.toSearch")}
            </p>
            <button
              type="button"
              onClick={onCreateNote}
              className="mt-5 px-4 h-9 rounded-[12px] text-[13px] font-medium text-white bg-[var(--accent)] shadow-[0_8px_22px_rgba(10,132,255,0.2)]"
            >
              新建笔记
            </button>
          </div>
        </div>
      )}
    </main>
  );
}

function deriveTitleFromBody(bodyText: string) {
  return bodyText
    .split(/\r?\n/)
    .map((line) => line.trim())
    .find((line) => line.startsWith("#") && line.replace(/^#+\s*/, "").trim())
    ?.replace(/^#+\s*/, "")
    .trim() ?? "";
}
