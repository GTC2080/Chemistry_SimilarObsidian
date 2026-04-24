import { useState, useRef, useEffect } from "react";
import logoSvg from "../assets/logo.svg";
import type { ActivityBarItemId } from "./settings/settingsTypes";
import { useT } from "../i18n";

interface ActivityBarProps {
  onOpenSearch: () => void;
  onOpenGraph: () => void;
  onToggleAI: () => void;
  onOpenKinetics: () => void;
  onCreateChemDraw: () => void;
  onInsertChemDraw: () => void;
  onBackToManager: () => void;
  onToggleTimeline: () => void;
  canOpenKinetics: boolean;
  kineticsOpen: boolean;
  timelineOpen: boolean;
  activePanel: string;
  visibleItems: ActivityBarItemId[];
  canInsertChemDraw: boolean;
}

/** 最左侧窄图标条 — 参考 Obsidian / VS Code Activity Bar */
export default function ActivityBar({
  onOpenSearch, onOpenGraph, onToggleAI, onOpenKinetics, onCreateChemDraw, onInsertChemDraw,
  onBackToManager, onToggleTimeline, canOpenKinetics, kineticsOpen, timelineOpen,
  activePanel: _, visibleItems, canInsertChemDraw,
}: ActivityBarProps) {
  const t = useT();
  const show = (id: ActivityBarItemId) => visibleItems.includes(id);
  const [chemMenu, setChemMenu] = useState(false);
  const chemMenuRef = useRef<HTMLDivElement>(null);

  useEffect(() => {
    if (!chemMenu) return;
    const close = (e: MouseEvent) => {
      if (chemMenuRef.current && !chemMenuRef.current.contains(e.target as Node)) {
        setChemMenu(false);
      }
    };
    document.addEventListener("mousedown", close);
    return () => document.removeEventListener("mousedown", close);
  }, [chemMenu]);

  return (
    <div className="w-[42px] shrink-0 flex flex-col items-center select-none app-chrome"
      style={{
        borderRight: "0.5px solid var(--chrome-border)",
      }}>
      {/* Logo / 返回首页 */}
      <button type="button" onClick={onBackToManager}
        className="w-full h-[42px] flex items-center justify-center cursor-pointer
          transition-colors duration-150 hover:bg-[var(--sidebar-hover)] active:scale-95"
        title={t("activityBar.backToManager")} aria-label={t("activityBar.backToManager")}>
        <img src={logoSvg} alt="" className="w-[20px] h-[20px] rounded-[4px]" />
      </button>

      {/* 分隔线 */}
      <div className="w-5 my-1" style={{ borderTop: "0.5px solid var(--separator-light)" }} />

      {show("search") && (
        <IconBtn onClick={onOpenSearch} title={`${t("activityBar.search")} (Ctrl+K)`} aria-label={t("activityBar.search")}>
          <circle cx="11" cy="11" r="8" /><line x1="21" y1="21" x2="16.65" y2="16.65" />
        </IconBtn>
      )}

      {show("graph") && (
        <IconBtn onClick={onOpenGraph} title={`${t("activityBar.graph")} (Ctrl+G)`} aria-label={t("activityBar.graph")}>
          <circle cx="12" cy="5" r="2" /><circle cx="5" cy="19" r="2" /><circle cx="19" cy="19" r="2" />
          <line x1="12" y1="7" x2="5" y2="17" /><line x1="12" y1="7" x2="19" y2="17" />
        </IconBtn>
      )}

      {show("ai") && (
        <IconBtn onClick={onToggleAI} title={`${t("activityBar.ai")} (Ctrl+J)`} aria-label={t("activityBar.ai")}>
          <path d="M21 15a2 2 0 0 1-2 2H7l-4 4V5a2 2 0 0 1 2-2h14a2 2 0 0 1 2 2z" />
        </IconBtn>
      )}

      {show("chemdraw") && (
        <div className="relative" ref={chemMenuRef}>
          <IconBtn onClick={() => setChemMenu((v) => !v)} title={t("activityBar.chemdraw")} aria-label={t("activityBar.chemdraw")} active={chemMenu}>
            <polygon points="12,3 19.5,7.5 19.5,16.5 12,21 4.5,16.5 4.5,7.5" fill="none" />
          </IconBtn>
          {chemMenu && (
            <div className="absolute left-[42px] top-0 z-50 min-w-[160px] py-1 rounded-lg border border-[var(--chrome-border)] shadow-xl"
              style={{ background: "var(--panel-bg)" }}>
              <button type="button"
                className="w-full px-3 py-1.5 text-left text-[12px] flex items-center gap-2 hover:bg-[var(--sidebar-hover)] transition-colors text-[var(--text-secondary)]"
                onClick={() => { onCreateChemDraw(); setChemMenu(false); }}>
                <svg className="w-3.5 h-3.5 shrink-0" viewBox="0 0 24 24" fill="none" stroke="currentColor" strokeWidth="2" strokeLinecap="round" strokeLinejoin="round">
                  <path d="M14 2H6a2 2 0 0 0-2 2v16a2 2 0 0 0 2 2h12a2 2 0 0 0 2-2V8z" />
                  <polyline points="14 2 14 8 20 8" />
                  <line x1="12" y1="18" x2="12" y2="12" /><line x1="9" y1="15" x2="15" y2="15" />
                </svg>
                {t("chemdraw.newFile")}
              </button>
              <button type="button"
                className="w-full px-3 py-1.5 text-left text-[12px] flex items-center gap-2 transition-colors hover:bg-[var(--sidebar-hover)]"
                style={{
                  color: canInsertChemDraw ? "var(--text-secondary)" : "var(--text-quaternary)",
                  cursor: canInsertChemDraw ? "pointer" : "not-allowed",
                }}
                onClick={() => { if (canInsertChemDraw) { onInsertChemDraw(); setChemMenu(false); } }}>
                <svg className="w-3.5 h-3.5 shrink-0" viewBox="0 0 24 24" fill="none" stroke="currentColor" strokeWidth="2" strokeLinecap="round" strokeLinejoin="round">
                  <path d="M21 15a2 2 0 0 1-2 2H7l-4 4V5a2 2 0 0 1 2-2h14a2 2 0 0 1 2 2z" />
                </svg>
                {t("chemdraw.insertToNote")}
              </button>
            </div>
          )}
        </div>
      )}

      {show("kinetics") && canOpenKinetics && (
        <IconBtn
          onClick={onOpenKinetics}
          title={t("activityBar.kinetics")}
          aria-label={t("activityBar.kinetics")}
          active={kineticsOpen}
        >
          <line x1="5" y1="19" x2="5" y2="11" />
          <line x1="12" y1="19" x2="12" y2="7" />
          <line x1="19" y1="19" x2="19" y2="4" />
          <circle cx="5" cy="9" r="1.5" />
          <circle cx="12" cy="5" r="1.5" />
          <circle cx="19" cy="2.5" r="1.5" />
        </IconBtn>
      )}

      {show("timeline") && (
        <IconBtn
          onClick={onToggleTimeline}
          title={t("activityBar.timeline")}
          aria-label={t("activityBar.timeline")}
          active={timelineOpen}
        >
          <circle cx="12" cy="12" r="10" />
          <polyline points="12 6 12 12 16 14" />
        </IconBtn>
      )}

    </div>
  );
}

/** 通用图标按钮 */
function IconBtn({ onClick, title, children, active = false, "aria-label": ariaLabel }: {
  onClick: () => void; title: string; children: React.ReactNode; active?: boolean; "aria-label": string;
}) {
  return (
    <button type="button" onClick={onClick} title={title} aria-label={ariaLabel}
      className="w-[32px] h-[32px] my-[2px] rounded-[6px] flex items-center justify-center
        cursor-pointer transition-colors duration-150
        hover:bg-[var(--sidebar-hover)] active:scale-95"
      style={{
        color: active ? "var(--accent)" : "var(--text-quaternary)",
        background: active ? "rgba(10,132,255,0.12)" : undefined,
      }}>
      <svg className="w-[18px] h-[18px]" viewBox="0 0 24 24" fill="none"
        stroke="currentColor" strokeWidth="1.8" strokeLinecap="round" strokeLinejoin="round">
        {children}
      </svg>
    </button>
  );
}
