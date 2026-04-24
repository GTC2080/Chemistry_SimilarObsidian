import { useT } from "../../i18n";

interface AppStatusBarProps {
  vaultPath: string;
  truthLevel: number;
  onOpenTruth: () => void;
  onOpenSettings: () => void;
}

export default function AppStatusBar({
  vaultPath,
  truthLevel,
  onOpenTruth,
  onOpenSettings,
}: AppStatusBarProps) {
  const t = useT();
  return (
    <div className="h-[28px] min-h-[28px] flex items-center justify-between px-3 select-none app-chrome
      border-t-[0.5px] border-t-[var(--chrome-border)]">
      <div className="flex items-center gap-2">
        <svg className="w-3 h-3 text-[var(--text-quinary)]"
          viewBox="0 0 24 24" fill="none" stroke="currentColor" strokeWidth="2"
          strokeLinecap="round" strokeLinejoin="round">
          <path d="M22 19a2 2 0 0 1-2 2H4a2 2 0 0 1-2-2V5a2 2 0 0 1 2-2h5l2 3h9a2 2 0 0 1 2 2z" />
        </svg>
        <span className="text-[11px] text-[var(--text-quaternary)]">
          {vaultPath.split(/[/\\]/).pop()}
        </span>
      </div>
      <div className="flex items-center gap-1">
        <button
          type="button"
          onClick={onOpenTruth}
          className="h-6 px-1.5 rounded-md flex items-center justify-center cursor-pointer
            transition-colors duration-150 hover:bg-[var(--sidebar-hover)]"
          title="TRUTH_SYSTEM"
          aria-label="TRUTH_SYSTEM"
        >
          <span className="font-mono text-[10px] tracking-wider text-[var(--text-quaternary)]">
            LVL.{String(truthLevel).padStart(2, "0")}
          </span>
        </button>
        <button
          type="button"
          onClick={onOpenSettings}
          className="w-6 h-6 rounded-md flex items-center justify-center cursor-pointer
            transition-colors duration-150 hover:bg-[var(--sidebar-hover)] text-[var(--text-quaternary)]"
          title={`${t("statusBar.settings")} (Ctrl+,)`}
          aria-label={t("statusBar.settings")}
        >
          <svg className="w-3.5 h-3.5" viewBox="0 0 24 24" fill="none"
            stroke="currentColor" strokeWidth="1.8" strokeLinecap="round" strokeLinejoin="round">
            <circle cx="12" cy="12" r="3" />
            <path d="M19.4 15a1.65 1.65 0 0 0 .33 1.82l.06.06a2 2 0 0 1-2.83 2.83l-.06-.06a1.65 1.65 0 0 0-1.82-.33 1.65 1.65 0 0 0-1 1.51V21a2 2 0 0 1-4 0v-.09A1.65 1.65 0 0 0 9 19.4a1.65 1.65 0 0 0-1.82.33l-.06.06a2 2 0 0 1-2.83-2.83l.06-.06A1.65 1.65 0 0 0 4.68 15a1.65 1.65 0 0 0-1.51-1H3a2 2 0 0 1 0-4h.09A1.65 1.65 0 0 0 4.6 9a1.65 1.65 0 0 0-.33-1.82l-.06-.06a2 2 0 0 1 2.83-2.83l.06.06A1.65 1.65 0 0 0 9 4.68a1.65 1.65 0 0 0 1-1.51V3a2 2 0 0 1 4 0v.09a1.65 1.65 0 0 0 1 1.51 1.65 1.65 0 0 0 1.82-.33l.06-.06a2 2 0 0 1 2.83 2.83l-.06.06A1.65 1.65 0 0 0 19.4 9a1.65 1.65 0 0 0 1.51 1H21a2 2 0 0 1 0 4h-.09a1.65 1.65 0 0 0-1.51 1z" />
          </svg>
        </button>
      </div>
    </div>
  );
}
