import { useEffect, useRef, useState } from "react";
import type { NoteInfo } from "../types";
import { useT } from "../i18n";
import { useAIChatStream } from "../hooks/useAIChatStream";
import ChatBubble from "./ai/ChatBubble";
import AIContextPanel from "./ai/AIContextPanel";

interface AIAssistantSidebarProps {
  width: number;
  relatedNotes: NoteInfo[];
  resonanceLoading: boolean;
  onSelectNote: (note: NoteInfo) => void;
  embedded?: boolean;
  activeNoteId?: string;
}

export default function AIAssistantSidebar({
  width,
  relatedNotes,
  resonanceLoading,
  onSelectNote,
  embedded,
  activeNoteId,
}: AIAssistantSidebarProps) {
  const t = useT();
  const [contextOpen, setContextOpen] = useState(true);
  const inputRef = useRef<HTMLTextAreaElement>(null);
  const scrollRef = useRef<HTMLDivElement>(null);

  const {
    renderedMessages,
    streamingMessage,
    input,
    streaming,
    handleSubmit,
    handleKeyDown,
    handleInputChange,
    clearConversation,
    cancelStreaming,
  } = useAIChatStream({ activeNoteId });

  useEffect(() => {
    if (scrollRef.current) {
      scrollRef.current.scrollTop = scrollRef.current.scrollHeight;
    }
  }, [renderedMessages, streamingMessage?.loading]);

  const hasContext = relatedNotes.length > 0 || resonanceLoading;

  return (
    <aside
      className="flex flex-col workspace-panel"
      style={{
        width: `var(--right-drag-width, ${width}px)`,
        minWidth: `var(--right-drag-width, ${width}px)`,
        ...(embedded ? { margin: "0" } : { margin: "0" }),
        borderLeft: "0.5px solid var(--panel-border)",
        borderRight: "none",
        overflow: "hidden",
      }}
    >
      <div
        className="px-4 py-3 flex items-center justify-between shrink-0"
        style={{ borderBottom: "0.5px solid var(--panel-border)" }}
      >
        <div className="flex items-center gap-2">
          <svg
            className="w-4 h-4"
            style={{ color: "var(--accent)" }}
            viewBox="0 0 24 24"
            fill="none"
            stroke="currentColor"
            strokeWidth="2"
            strokeLinecap="round"
            strokeLinejoin="round"
          >
            <path d="M21 15a2 2 0 0 1-2 2H7l-4 4V5a2 2 0 0 1 2-2h14a2 2 0 0 1 2 2z" />
          </svg>
          <span className="text-[13px] font-semibold" style={{ color: "var(--text-primary)" }}>
            {t("ai.title")}
          </span>
        </div>
        {(renderedMessages.length > 0 || streamingMessage) && (
          <button
            type="button"
            onClick={clearConversation}
            className="text-[11px] px-2 py-1 rounded-[7px] transition-all duration-150 cursor-pointer hover:bg-[var(--sidebar-hover)]"
            style={{ color: "var(--text-quaternary)", background: "var(--subtle-surface-strong)" }}
          >
            {t("ai.clear")}
          </button>
        )}
      </div>

      <div ref={scrollRef} className="flex-1 overflow-y-auto px-3 py-3 space-y-3 min-h-0">
        {renderedMessages.length === 0 && (
          <div className="flex flex-col items-center py-14 gap-3">
            <div
              className="w-12 h-12 rounded-[16px] flex items-center justify-center"
              style={{ background: "var(--subtle-surface)" }}
            >
              <svg
                className="w-6 h-6"
                style={{ color: "var(--text-quinary)" }}
                viewBox="0 0 24 24"
                fill="none"
                stroke="currentColor"
                strokeWidth="1.5"
                strokeLinecap="round"
                strokeLinejoin="round"
              >
                <path d="M21 15a2 2 0 0 1-2 2H7l-4 4V5a2 2 0 0 1 2-2h14a2 2 0 0 1 2 2z" />
              </svg>
            </div>
            <p
              className="text-[12px] text-center leading-relaxed max-w-[200px] whitespace-pre-line"
              style={{ color: "var(--text-quaternary)" }}
            >
              {t("ai.emptyHint")}
            </p>
            <p className="text-[11px] text-center" style={{ color: "var(--text-quinary)" }}>
              {t("ai.enterHint")}
            </p>
          </div>
        )}

        {renderedMessages.map(message => {
          const isStreamingBubble = streamingMessage && message.id === streamingMessage.id;
          return (
            <ChatBubble
              key={message.id}
              role={message.role}
              content={message.content}
              loading={isStreamingBubble ? streamingMessage.loading : false}
            />
          );
        })}
      </div>

      {hasContext && (
        <AIContextPanel
          relatedNotes={relatedNotes}
          resonanceLoading={resonanceLoading}
          onSelectNote={onSelectNote}
          contextOpen={contextOpen}
          onToggleContext={() => setContextOpen(prev => !prev)}
        />
      )}

      <div
        className="shrink-0 px-3 pb-3 pt-2"
        style={{ borderTop: hasContext ? "none" : "0.5px solid var(--panel-border)" }}
      >
        <div
          className="flex items-end gap-2 rounded-[14px] px-3.5 py-2.5 transition-all duration-200"
          style={{
            background: "var(--subtle-surface)",
            border: "0.5px solid var(--panel-border)",
            boxShadow: "inset 0 1px 3px rgba(0,0,0,0.15)",
          }}
        >
          <textarea
            ref={inputRef}
            value={input}
            onChange={handleInputChange}
            onKeyDown={handleKeyDown}
            placeholder={streaming ? t("ai.streaming") : t("ai.placeholder")}
            disabled={streaming}
            rows={1}
            className="flex-1 bg-transparent text-[13px] leading-relaxed outline-none resize-none disabled:opacity-50 placeholder:text-[var(--text-quaternary)]"
            style={{
              color: "var(--text-primary)",
              caretColor: "var(--accent)",
              maxHeight: "96px",
            }}
          />
          {streaming ? (
            <button
              type="button"
              onClick={cancelStreaming}
              aria-label={t("ai.cancel") || "Cancel"}
              className="apple-btn w-7 h-7 rounded-[9px] flex items-center justify-center shrink-0 transition-all cursor-pointer"
              style={{
                background: "rgba(255,69,58,0.15)",
                boxShadow: "0 2px 6px rgba(255,69,58,0.2)",
              }}
            >
              <svg className="w-3 h-3" style={{ color: "rgb(255,69,58)" }}
                viewBox="0 0 24 24" fill="currentColor">
                <rect x="4" y="4" width="16" height="16" rx="2" />
              </svg>
            </button>
          ) : (
            <button
              type="button"
              onClick={() => {
                void handleSubmit();
              }}
              disabled={!input.trim()}
              aria-label={t("ai.send")}
              className="apple-btn w-7 h-7 rounded-[9px] flex items-center justify-center shrink-0 transition-all cursor-pointer disabled:opacity-25"
              style={{
                background: input.trim()
                  ? "linear-gradient(135deg, #0A84FF 0%, #0070E0 100%)"
                  : "var(--subtle-surface-strong)",
                boxShadow: input.trim() ? "0 2px 6px rgba(10,132,255,0.3)" : "none",
              }}
            >
              <svg
                className="w-3.5 h-3.5 text-white"
                viewBox="0 0 24 24"
                fill="none"
                stroke="currentColor"
                strokeWidth="2.5"
                strokeLinecap="round"
                strokeLinejoin="round"
              >
                <line x1="22" y1="2" x2="11" y2="13" />
                <polygon points="22 2 15 22 11 13 2 9 22 2" />
              </svg>
            </button>
          )}
        </div>
        <div className="flex items-center gap-2 mt-1.5 px-1">
          <span className="text-[10px]" style={{ color: "var(--text-quinary)" }}>
            {streaming ? "Esc to cancel" : t("ai.enterToSend")}
          </span>
          {!streaming && (
            <span className="text-[10px]" style={{ color: "var(--text-quinary)" }}>
              {t("ai.shiftEnterNewLine")}
            </span>
          )}
        </div>
      </div>
    </aside>
  );
}
