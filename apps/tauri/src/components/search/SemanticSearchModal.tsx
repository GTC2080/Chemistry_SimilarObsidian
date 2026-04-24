import { useState, useEffect, useRef, useCallback } from "react";
import { invoke } from "@tauri-apps/api/core";
import type { NoteInfo } from "../../types";
import { perf } from "../../utils/perf";
import { useT } from "../../i18n";

interface SemanticSearchModalProps {
  open: boolean; onClose: () => void; onSelect: (note: NoteInfo) => void;
}

export default function SemanticSearchModal({ open, onClose, onSelect }: SemanticSearchModalProps) {
  const t = useT();
  const [query, setQuery] = useState("");
  const [results, setResults] = useState<NoteInfo[]>([]);
  const [selectedIndex, setSelectedIndex] = useState(0);
  const [searching, setSearching] = useState(false);
  const inputRef = useRef<HTMLInputElement>(null);
  const debounceRef = useRef<ReturnType<typeof setTimeout> | null>(null);

  useEffect(() => {
    if (open) {
      setQuery(""); setResults([]); setSelectedIndex(0); setSearching(false);
      requestAnimationFrame(() => inputRef.current?.focus());
    }
  }, [open]);

  const doSearch = useCallback((text: string) => {
    if (debounceRef.current) clearTimeout(debounceRef.current);
    if (!text.trim()) { setResults([]); setSearching(false); return; }
    setSearching(true);
    debounceRef.current = setTimeout(async () => {
      const endSearch = perf.start("semantic-search");
      try {
        const res = await invoke<NoteInfo[]>("semantic_search", { query: text, limit: 10 });
        setResults(res); setSelectedIndex(0);
      } catch { setResults([]); }
      finally { endSearch(); setSearching(false); }
    }, 500);
  }, []);

  useEffect(() => () => { if (debounceRef.current) clearTimeout(debounceRef.current); }, []);

  function handleKeyDown(e: React.KeyboardEvent) {
    if (e.key === "ArrowDown") { e.preventDefault(); setSelectedIndex(p => p >= results.length - 1 ? 0 : p + 1); }
    else if (e.key === "ArrowUp") { e.preventDefault(); setSelectedIndex(p => p <= 0 ? results.length - 1 : p - 1); }
    else if (e.key === "Enter") { e.preventDefault(); if (results[selectedIndex]) { onSelect(results[selectedIndex]); onClose(); } }
    else if (e.key === "Escape") { onClose(); }
  }

  if (!open) return null;

  return (
    <div className="fixed inset-0 z-50 flex items-start justify-center pt-[16vh]" onClick={onClose}>
      <div className="absolute inset-0" style={{ background: "rgba(0,0,0,0.55)", backdropFilter: "blur(8px)" }} />

      <div className="animate-modal-in glass-elevated glass-highlight relative w-full max-w-[560px] rounded-[18px] overflow-hidden"
        onClick={e => e.stopPropagation()}>

        <div className="flex items-center px-5 gap-3">
          <svg className="w-4 h-4 shrink-0" style={{ color: "var(--text-quaternary)" }}
            viewBox="0 0 24 24" fill="none" stroke="currentColor" strokeWidth="2" strokeLinecap="round" strokeLinejoin="round">
            <circle cx="11" cy="11" r="8" /><line x1="21" y1="21" x2="16.65" y2="16.65" />
          </svg>
          <input ref={inputRef} type="text" value={query}
            onChange={e => { setQuery(e.target.value); doSearch(e.target.value); }}
            onKeyDown={handleKeyDown}
            placeholder={t("search.placeholder")}
            className="flex-1 bg-transparent text-[14px] py-4 outline-none"
            style={{ color: "var(--text-primary)", caretColor: "var(--accent)" }}
          />
          <kbd className="text-[10px] px-1.5 py-0.5 rounded-md font-mono"
            style={{ background: "rgba(118,118,128,0.12)", color: "var(--text-quaternary)" }}>ESC</kbd>
        </div>

        <div style={{ height: "0.5px", background: "rgba(255,255,255,0.06)" }} />

        <div className="max-h-[320px] overflow-y-auto py-1">
          {searching && (
            <div className="flex items-center justify-center py-10 gap-2.5">
              <div className="w-4 h-4 rounded-full border-2 animate-spin"
                style={{ borderColor: "rgba(10,132,255,0.12)", borderTopColor: "var(--accent)" }} />
              <span className="text-[13px]" style={{ color: "var(--text-tertiary)" }}>{t("search.searching")}</span>
            </div>
          )}

          {!searching && query.trim() && results.length === 0 && (
            <div className="py-10 text-center">
              <p className="text-[13px]" style={{ color: "var(--text-quaternary)" }}>{t("search.noResults")}</p>
            </div>
          )}

          {!searching && results.map((note, index) => {
            const sel = index === selectedIndex;
            return (
              <button key={note.id} type="button"
                onClick={() => { onSelect(note); onClose(); }}
                className="w-full text-left px-4 py-2.5 mx-1 flex items-center gap-3
                  transition-all duration-150 cursor-pointer rounded-[10px]"
                style={{
                  width: "calc(100% - 8px)",
                  background: sel ? "rgba(10,132,255,0.12)" : "transparent",
                  color: sel ? "var(--text-primary)" : "var(--text-secondary)",
                }}>
                <svg className="w-[14px] h-[14px] shrink-0"
                  style={{ color: sel ? "var(--accent)" : "var(--text-quaternary)" }}
                  viewBox="0 0 24 24" fill="none" stroke="currentColor" strokeWidth="1.5"
                  strokeLinecap="round" strokeLinejoin="round">
                  <path d="M14 2H6a2 2 0 0 0-2 2v16a2 2 0 0 0 2 2h12a2 2 0 0 0 2-2V8z" />
                  <polyline points="14 2 14 8 20 8" />
                </svg>
                <span className="text-[13px] truncate">{note.name}</span>
              </button>
            );
          })}
        </div>

        {results.length > 0 && !searching && (
          <div className="px-5 py-2.5 flex items-center gap-4 text-[11px]"
            style={{ borderTop: "0.5px solid rgba(255,255,255,0.05)", color: "var(--text-quaternary)" }}>
            <span>{t("search.navHint")}</span>
            <span>{t("search.openHint")}</span>
          </div>
        )}
      </div>
    </div>
  );
}
