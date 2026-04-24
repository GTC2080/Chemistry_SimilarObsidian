import { useT } from "../../i18n";

interface SidebarHeaderProps {
  vaultPath: string;
  tab: "files" | "tags";
  tagsCount: number;
  newMenuOpen: boolean;
  onToggleNewMenu: () => void;
  onSelectTab: (tab: "files" | "tags") => void;
  onCreateFile: (kind: "note" | "mol" | "paper", targetFolderRelativePath?: string) => void;
  onCreateFolder: (targetParentRelativePath?: string) => void;
}

export default function SidebarHeader({
  vaultPath,
  tab,
  tagsCount,
  newMenuOpen,
  onToggleNewMenu,
  onSelectTab,
  onCreateFile,
  onCreateFolder,
}: SidebarHeaderProps) {
  const t = useT();
  return (
    <div className="px-3 pt-3 pb-2">
      <div className="flex items-center justify-between mb-2 relative">
        <span
          className="text-[13px] font-semibold truncate mr-2"
          style={{ color: "var(--text-primary)" }}
          title={vaultPath}
        >
          {vaultPath.replace(/[\\/]+$/, "").split(/[\\/]/).pop() || "Vault"}
        </span>
        <button
          type="button"
          onClick={onToggleNewMenu}
          className="w-7 h-7 rounded-md text-[15px] cursor-pointer hover:bg-[var(--sidebar-hover)] transition-colors"
          style={{ color: "var(--text-tertiary)" }}
          title={t("sidebar.new")}
          aria-label={t("sidebar.new")}
        >
          +
        </button>
        {newMenuOpen && (
          <div
            className="absolute top-8 right-0 z-20 rounded-lg p-1 min-w-[132px]"
            style={{ background: "var(--menu-bg)", border: "1px solid var(--separator-light)" }}
          >
            <button
              type="button"
              onClick={() => onCreateFile("note", "")}
              className="w-full text-left px-2.5 py-1.5 text-[12px] rounded-md transition-colors hover:bg-[var(--menu-hover)]"
              style={{ color: "var(--text-secondary)" }}
            >
              {t("sidebar.newNote")}
            </button>
            <button
              type="button"
              onClick={() => onCreateFile("mol", "")}
              className="w-full text-left px-2.5 py-1.5 text-[12px] rounded-md transition-colors hover:bg-[var(--menu-hover)]"
              style={{ color: "var(--text-secondary)" }}
            >
              {t("sidebar.newMol")}
            </button>
            <button
              type="button"
              onClick={() => onCreateFile("paper", "")}
              className="w-full text-left px-2.5 py-1.5 text-[12px] rounded-md transition-colors hover:bg-[var(--menu-hover)]"
              style={{ color: "var(--text-secondary)" }}
            >
              {t("sidebar.newPaper")}
            </button>
            <button
              type="button"
              onClick={() => onCreateFolder("")}
              className="w-full text-left px-2.5 py-1.5 text-[12px] rounded-md transition-colors hover:bg-[var(--menu-hover)]"
              style={{ color: "var(--text-secondary)" }}
            >
              {t("sidebar.newFolder")}
            </button>
          </div>
        )}
      </div>
      <div
        className="flex p-[3px] rounded-[10px]"
        style={{
          background: "var(--subtle-surface)",
          border: "0.5px solid var(--panel-border)",
        }}
      >
        {(["files", "tags"] as const).map(nextTab => {
          const active = tab === nextTab;
          return (
            <button
              key={nextTab}
              onClick={() => onSelectTab(nextTab)}
              className="flex-1 px-2 py-[5px] rounded-[9px] text-[12px] font-medium transition-colors duration-250 cursor-pointer flex items-center justify-center gap-1.5"
              style={{
                background: active ? "rgba(10,132,255,0.16)" : "transparent",
                color: active ? "var(--text-primary)" : "var(--text-tertiary)",
                boxShadow: active
                  ? "0 1px 4px rgba(0,0,0,0.25), 0 0.5px 1px rgba(0,0,0,0.15), inset 0 0.5px 0 rgba(255,255,255,0.08)"
                  : "none",
              }}
            >
              {nextTab === "files" ? (
                <svg
                  className="w-3.5 h-3.5"
                  viewBox="0 0 24 24"
                  fill="none"
                  stroke="currentColor"
                  strokeWidth="1.8"
                  strokeLinecap="round"
                  strokeLinejoin="round"
                >
                  <path d="M22 19a2 2 0 0 1-2 2H4a2 2 0 0 1-2-2V5a2 2 0 0 1 2-2h5l2 3h9a2 2 0 0 1 2 2z" />
                </svg>
              ) : (
                <svg
                  className="w-3.5 h-3.5"
                  viewBox="0 0 24 24"
                  fill="none"
                  stroke="currentColor"
                  strokeWidth="1.8"
                  strokeLinecap="round"
                  strokeLinejoin="round"
                >
                  <path d="M20.59 13.41l-7.17 7.17a2 2 0 0 1-2.83 0L2 12V2h10l8.59 8.59a2 2 0 0 1 0 2.82z" />
                  <line x1="7" y1="7" x2="7.01" y2="7" />
                </svg>
              )}
              {nextTab === "files" ? t("sidebar.files") : t("sidebar.tags")}
              {nextTab === "tags" && tagsCount > 0 && (
                <span className="text-[10px]" style={{ color: "var(--text-quaternary)" }}>{tagsCount}</span>
              )}
            </button>
          );
        })}
      </div>
    </div>
  );
}
