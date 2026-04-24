import type { MouseEvent } from "react";
import logoSvg from "../../assets/logo.svg";
import { useT } from "../../i18n";

interface AppTitleBarProps {
  onBackgroundMouseDown: (e: MouseEvent<HTMLDivElement>) => void;
  onBackgroundDoubleClick: (e: MouseEvent<HTMLDivElement>) => void;
  onMinimize: () => void | Promise<void>;
  onToggleMaximize: () => void | Promise<void>;
  onClose: () => void | Promise<void>;
}

export default function AppTitleBar({
  onBackgroundMouseDown,
  onBackgroundDoubleClick,
  onMinimize,
  onToggleMaximize,
  onClose,
}: AppTitleBarProps) {
  const t = useT();
  return (
    <div
      onMouseDown={onBackgroundMouseDown}
      onDoubleClick={onBackgroundDoubleClick}
      className="h-[34px] min-h-[34px] flex items-center justify-between select-none app-chrome
        border-b-[0.5px] border-b-[var(--chrome-border)] shadow-[0_1px_0_rgba(0,0,0,0.25)]"
    >
      <div className="flex items-center gap-2 pl-4">
        <img src={logoSvg} alt="Logo" className="w-[16px] h-[16px] rounded-[3px]" />
        <span className="text-[12px] font-medium text-[var(--text-tertiary)]">
          Nexus
        </span>
      </div>
      <div className="flex items-center h-full text-[var(--text-tertiary)]">
        <button
          onClick={onMinimize}
          className="h-full w-10 flex items-center justify-center transition-colors duration-150 cursor-pointer hover:bg-[var(--sidebar-hover)] rounded-none"
          aria-label={t("titleBar.minimize")}
        >
          <svg width="10" height="1" viewBox="0 0 10 1" fill="currentColor"><rect width="10" height="1" /></svg>
        </button>
        <button
          onClick={onToggleMaximize}
          className="h-full w-10 flex items-center justify-center transition-colors duration-150 cursor-pointer hover:bg-[var(--sidebar-hover)] rounded-none"
          aria-label={t("titleBar.maximize")}
        >
          <svg width="10" height="10" viewBox="0 0 10 10" fill="none" stroke="currentColor" strokeWidth="1"><rect x="0.5" y="0.5" width="9" height="9" rx="1" /></svg>
        </button>
        <button
          onClick={onClose}
          className="h-full w-12 flex items-center justify-center transition-colors duration-150 cursor-pointer hover:bg-[#ff453a]/90 hover:text-white rounded-none"
          aria-label={t("titleBar.close")}
        >
          <svg width="10" height="10" viewBox="0 0 10 10" fill="none" stroke="currentColor" strokeWidth="1.2">
            <line x1="1" y1="1" x2="9" y2="9" /><line x1="9" y1="1" x2="1" y2="9" />
          </svg>
        </button>
      </div>
    </div>
  );
}
