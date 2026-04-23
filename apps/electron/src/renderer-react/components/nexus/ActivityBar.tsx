import type { ReactNode } from "react";
import logoSvg from "../../assets/logo.svg";
import { useT } from "../../i18n";
import type { WorkspacePanel } from "../workspace/workspace-types";

interface ActivityBarProps {
  onBackToManager: () => void;
  activePanel: WorkspacePanel;
  onSelectPanel: (panel: WorkspacePanel) => void;
  visibleItems: WorkspacePanel[];
}

interface ActivityItem {
  id: WorkspacePanel;
  label: string;
  title: string;
  icon: ReactNode;
}

export default function ActivityBar({
  onBackToManager,
  activePanel,
  onSelectPanel,
  visibleItems
}: ActivityBarProps) {
  const t = useT();
  const items: ActivityItem[] = [
    {
      id: "files",
      label: "目录",
      title: "文件与笔记",
      icon: (
        <>
          <path d="M3 7.5A1.5 1.5 0 0 1 4.5 6H9l1.8 2H19.5A1.5 1.5 0 0 1 21 9.5v8A1.5 1.5 0 0 1 19.5 19h-15A1.5 1.5 0 0 1 3 17.5z" />
        </>
      )
    },
    {
      id: "search",
      label: t("activityBar.search"),
      title: `${t("activityBar.search")} (Ctrl+K)`,
      icon: (
        <>
          <circle cx="11" cy="11" r="8" />
          <line x1="21" y1="21" x2="16.65" y2="16.65" />
        </>
      )
    },
    {
      id: "attachments",
      label: "附件",
      title: "附件目录",
      icon: (
        <>
          <path d="M9 7.5v8.25a3.75 3.75 0 1 0 7.5 0V6.5a2.5 2.5 0 0 0-5 0v8.75a1.25 1.25 0 1 0 2.5 0V8.5" />
        </>
      )
    },
    {
      id: "pdf",
      label: "PDF",
      title: "PDF 元数据与引用",
      icon: (
        <>
          <path d="M14 3H7a2 2 0 0 0-2 2v14a2 2 0 0 0 2 2h10a2 2 0 0 0 2-2V8z" />
          <polyline points="14 3 14 8 19 8" />
          <path d="M8.5 13.5h2.4a1.4 1.4 0 0 0 0-2.8H8.5v5.8" />
          <path d="M13.8 16.5h1.1a1.9 1.9 0 0 0 0-3.8h-1.1z" />
        </>
      )
    },
    {
      id: "chemistry",
      label: "化学",
      title: "谱图与化学引用",
      icon: (
        <>
          <path d="M9 3v5l-4.6 7.6A3 3 0 0 0 7 20h10a3 3 0 0 0 2.6-4.4L15 8V3" />
          <path d="M9 8h6" />
          <path d="M8 14h8" />
        </>
      )
    },
    {
      id: "diagnostics",
      label: "诊断",
      title: "诊断与 support bundle",
      icon: (
        <>
          <polyline points="4 13 8 13 10.5 6 13.5 18 16 11 20 11" />
        </>
      )
    },
    {
      id: "rebuild",
      label: "重建",
      title: "重建生命周期",
      icon: (
        <>
          <path d="M20 11a8 8 0 1 0 2 5.3" />
          <polyline points="20 4 20 11 13 11" />
        </>
      )
    }
  ];

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

      <ActivityGroup>
        {items
          .filter((item) => visibleItems.includes(item.id) && (item.id === "files" || item.id === "search"))
          .map((item) => (
            <IconBtn
              key={item.id}
              onClick={() => onSelectPanel(item.id)}
              title={item.title}
              aria-label={item.label}
              active={activePanel === item.id}
            >
              {item.icon}
            </IconBtn>
          ))}
      </ActivityGroup>

      <ActivityGroup>
        {items
          .filter((item) => visibleItems.includes(item.id) && (item.id === "attachments" || item.id === "pdf" || item.id === "chemistry"))
          .map((item) => (
            <IconBtn
              key={item.id}
              onClick={() => onSelectPanel(item.id)}
              title={item.title}
              aria-label={item.label}
              active={activePanel === item.id}
            >
              {item.icon}
            </IconBtn>
          ))}
      </ActivityGroup>

      <div className="flex-1" />

      <ActivityGroup last>
        {items
          .filter((item) => visibleItems.includes(item.id) && (item.id === "diagnostics" || item.id === "rebuild"))
          .map((item) => (
            <IconBtn
              key={item.id}
              onClick={() => onSelectPanel(item.id)}
              title={item.title}
              aria-label={item.label}
              active={activePanel === item.id}
            >
              {item.icon}
            </IconBtn>
          ))}
      </ActivityGroup>
    </div>
  );
}

function ActivityGroup({ children, last = false }: { children: ReactNode; last?: boolean }) {
  return (
    <div
      className={`flex flex-col items-center ${last ? "mb-2" : "mb-1"}`}
      style={last ? undefined : { borderBottom: "0.5px solid var(--separator-light)", paddingBottom: 5 }}
    >
      {children}
    </div>
  );
}

function IconBtn({
  onClick,
  title,
  children,
  active = false,
  "aria-label": ariaLabel
}: {
  onClick: () => void;
  title: string;
  children: ReactNode;
  active?: boolean;
  "aria-label": string;
}) {
  return (
    <button
      type="button"
      onClick={onClick}
      title={title}
      aria-label={ariaLabel}
      className="relative w-[32px] h-[32px] my-[2px] rounded-[8px] flex items-center justify-center cursor-pointer transition-colors duration-150 hover:bg-[var(--sidebar-hover)] active:scale-95"
      style={{
        color: active ? "var(--accent)" : "var(--text-quaternary)",
        background: active ? "rgba(10,132,255,0.12)" : undefined
      }}
    >
      {active ? (
        <span
          className="absolute left-[-5px] top-1/2 -translate-y-1/2 w-[3px] h-[16px] rounded-full"
          style={{ background: "var(--accent)", boxShadow: "0 0 8px rgba(10,132,255,0.45)" }}
        />
      ) : null}
      <svg
        className="w-[18px] h-[18px]"
        viewBox="0 0 24 24"
        fill="none"
        stroke="currentColor"
        strokeWidth="1.8"
        strokeLinecap="round"
        strokeLinejoin="round"
      >
        {children}
      </svg>
    </button>
  );
}
