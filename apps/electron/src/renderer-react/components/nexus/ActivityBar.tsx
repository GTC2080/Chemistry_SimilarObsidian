import logoSvg from "../../assets/logo.svg";
import { useT } from "../../i18n";

type VisibleItem = "search";

interface ActivityBarProps {
  onBackToManager: () => void;
  onOpenSearch: () => void;
  activePanel: "files" | "search";
  visibleItems: VisibleItem[];
}

export default function ActivityBar({
  onBackToManager,
  onOpenSearch,
  activePanel,
  visibleItems
}: ActivityBarProps) {
  const t = useT();
  const show = (id: VisibleItem) => visibleItems.includes(id);

  return (
    <div
      className="w-[42px] shrink-0 flex flex-col items-center select-none app-chrome"
      style={{ borderRight: "0.5px solid var(--chrome-border)" }}
    >
      <button
        type="button"
        onClick={onBackToManager}
        className="w-full h-[42px] flex items-center justify-center cursor-pointer transition-colors duration-150 hover:bg-[var(--sidebar-hover)] active:scale-95"
        title={t("activityBar.backToManager")}
        aria-label={t("activityBar.backToManager")}
      >
        <img src={logoSvg} alt="" className="w-[20px] h-[20px] rounded-[4px]" />
      </button>

      <div className="w-5 my-1" style={{ borderTop: "0.5px solid var(--separator-light)" }} />

      {show("search") && (
        <IconBtn onClick={onOpenSearch} title={`${t("activityBar.search")} (Ctrl+K)`} aria-label={t("activityBar.search")} active={activePanel === "search"}>
          <circle cx="11" cy="11" r="8" /><line x1="21" y1="21" x2="16.65" y2="16.65" />
        </IconBtn>
      )}
    </div>
  );
}

function IconBtn({ onClick, title, children, active = false, "aria-label": ariaLabel }: {
  onClick: () => void;
  title: string;
  children: React.ReactNode;
  active?: boolean;
  "aria-label": string;
}) {
  return (
    <button
      type="button"
      onClick={onClick}
      title={title}
      aria-label={ariaLabel}
      className="w-[32px] h-[32px] my-[2px] rounded-[6px] flex items-center justify-center cursor-pointer transition-colors duration-150 hover:bg-[var(--sidebar-hover)] active:scale-95"
      style={{
        color: active ? "var(--accent)" : "var(--text-quaternary)",
        background: active ? "rgba(10,132,255,0.12)" : undefined
      }}
    >
      <svg className="w-[18px] h-[18px]" viewBox="0 0 24 24" fill="none" stroke="currentColor" strokeWidth="1.8" strokeLinecap="round" strokeLinejoin="round">
        {children}
      </svg>
    </button>
  );
}
