import { forwardRef, useEffect, useImperativeHandle, useState } from "react";
import type { NoteInfo } from "../../types";
import { useT } from "../../i18n";

export interface SuggestionMenuProps {
  items: NoteInfo[];
  command: (item: NoteInfo) => void;
}

export interface SuggestionMenuRef {
  onKeyDown: (event: { event: KeyboardEvent }) => boolean;
}

const SuggestionMenu = forwardRef<SuggestionMenuRef, SuggestionMenuProps>(
  ({ items, command }, ref) => {
    const t = useT();
    const [selectedIndex, setSelectedIndex] = useState(0);
    useEffect(() => { setSelectedIndex(0); }, [items]);

    useImperativeHandle(ref, () => ({
      onKeyDown({ event }: { event: KeyboardEvent }) {
        if (event.key === "ArrowUp") { setSelectedIndex(p => p <= 0 ? items.length - 1 : p - 1); return true; }
        if (event.key === "ArrowDown") { setSelectedIndex(p => p >= items.length - 1 ? 0 : p + 1); return true; }
        if (event.key === "Enter") { if (items[selectedIndex]) command(items[selectedIndex]); return true; }
        return false;
      },
    }));

    if (items.length === 0) {
      return (
        <div className="glass-elevated rounded-xl p-3 text-[12px]" style={{ color: "var(--text-tertiary)" }}>
          {t("suggestion.noMatch")}
        </div>
      );
    }

    return (
      <div className="glass-elevated glass-highlight rounded-xl overflow-hidden min-w-[200px] max-w-[320px]">
        {items.map((item, index) => {
          const sel = index === selectedIndex;
          return (
            <button key={item.id} type="button" onClick={() => command(item)}
              className="w-full text-left px-3 py-2 text-[13px] transition-colors duration-150 cursor-pointer flex items-center gap-2"
              style={{
                background: sel ? "var(--accent-soft)" : "transparent",
                color: sel ? "var(--text-primary)" : "var(--text-secondary)"
              }}>
              <svg className="w-3 h-3 shrink-0"
                style={{ color: sel ? "var(--accent)" : "var(--text-quaternary)" }}
                viewBox="0 0 24 24" fill="none" stroke="currentColor" strokeWidth="1.5" strokeLinecap="round" strokeLinejoin="round">
                <path d="M14 2H6a2 2 0 0 0-2 2v16a2 2 0 0 0 2 2h12a2 2 0 0 0 2-2V8z" />
                <polyline points="14 2 14 8 20 8" />
              </svg>
              <span className="truncate">{item.name}</span>
            </button>
          );
        })}
      </div>
    );
  }
);

SuggestionMenu.displayName = "SuggestionMenu";
export default SuggestionMenu;
