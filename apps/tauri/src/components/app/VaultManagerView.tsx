import { useCallback, useRef, useState } from "react";
import type { MouseEvent as ReactMouseEvent } from "react";
import { createPortal } from "react-dom";
import logoSvg from "../../assets/logo.svg";
import type { RecentVault } from "../../types/vault";
import { useAppVersion } from "../../hooks/useAppVersion";
import { useContextMenuDismiss } from "../../hooks/useContextMenuDismiss";
import { useT } from "../../i18n";

const RECENT_MENU_WIDTH = 196;
const RECENT_MENU_MAX_HEIGHT = 120;

interface RecentVaultContextMenuState {
  x: number;
  y: number;
  vault: RecentVault;
}

interface VaultManagerViewProps {
  recentVaults: RecentVault[];
  onOpenRecent: (path: string) => void | Promise<void>;
  onRemoveRecent: (path: string) => void | Promise<void>;
  onOpenVault: () => void | Promise<void>;
  onOpenSettings: () => void;
  onOpenTruth: () => void;
}

export default function VaultManagerView({
  recentVaults,
  onOpenRecent,
  onRemoveRecent,
  onOpenVault,
  onOpenSettings,
  onOpenTruth,
}: VaultManagerViewProps) {
  const t = useT();
  const appVersion = useAppVersion();
  const [contextMenu, setContextMenu] = useState<RecentVaultContextMenuState | null>(null);
  const contextMenuRef = useRef<HTMLDivElement | null>(null);
  const closeContextMenu = useCallback(() => setContextMenu(null), []);
  useContextMenuDismiss(!!contextMenu, contextMenuRef, closeContextMenu);

  const handleRecentContextMenu = useCallback((e: ReactMouseEvent<HTMLButtonElement>, vault: RecentVault) => {
    e.preventDefault();
    e.stopPropagation();
    setContextMenu({
      x: e.clientX,
      y: e.clientY,
      vault,
    });
  }, []);

  const handleRemoveRecent = useCallback((vault: RecentVault) => {
    const ok = window.confirm(t("vaultManager.confirmRemoveRecent", { name: vault.name }));
    if (!ok) return;
    void onRemoveRecent(vault.path);
    setContextMenu(null);
  }, [onRemoveRecent]);

  return (
    <>
      <div className="flex flex-1 min-h-0">
      <aside
        className="w-64 flex flex-col select-none shrink-0"
        style={{ background: "var(--sidebar-bg)", borderRight: "0.5px solid var(--separator-light)" }}
      >
        <div className="px-4 pt-5 pb-3">
          <span className="text-[11px] font-semibold uppercase tracking-wider text-[var(--text-quaternary)]">
            {t("vaultManager.recentVaults")}
          </span>
        </div>
        <div className="flex-1 overflow-y-auto">
          {recentVaults.length === 0 ? (
            <div className="px-4 py-8">
              <p className="text-[12px] leading-relaxed text-[var(--text-quaternary)]">
                {t("vaultManager.emptyRecent")}
              </p>
            </div>
          ) : (
            recentVaults.map(vault => (
              <button
                key={vault.path}
                type="button"
                onClick={() => { void onOpenRecent(vault.path); }}
                onContextMenu={e => {
                  handleRecentContextMenu(e, vault);
                }}
                className="w-full text-left px-4 py-3 cursor-pointer transition-colors duration-150
                  hover:bg-[var(--sidebar-hover)] flex flex-col gap-1 border-l-2 border-l-transparent hover:border-l-[var(--accent)]"
              >
                <span className="text-[13px] font-medium text-[var(--text-secondary)]">
                  {vault.name}
                </span>
                <span className="text-[11px] truncate block text-[var(--text-quaternary)]">
                  {vault.path}
                </span>
              </button>
            ))
          )}
        </div>
      </aside>

      <main className="flex-1 flex flex-col items-center justify-center px-8 bg-[var(--surface-1)]">
        <div className="max-w-xl w-full animate-fade-in">
          <div className="flex flex-col items-center mb-10">
            <img src={logoSvg} alt="Nexus" className="w-20 h-20 rounded-[18px] mb-4" />
            <h1 className="text-2xl font-bold text-[var(--text-primary)]">
              Nexus
            </h1>
            <span className="text-[12px] mt-1 text-[var(--text-quaternary)]">
              {t("vaultManager.version")} {appVersion}
            </span>
          </div>

          <div className="flex flex-col gap-3 w-full">
            {[
              {
                title: t("vaultManager.openLocal"),
                desc: t("vaultManager.openLocalDesc"),
                btnLabel: t("vaultManager.open"),
                onClick: () => { void onOpenVault(); },
                primary: true,
              },
              {
                title: t("vaultManager.newVault"),
                desc: t("vaultManager.newVaultDesc"),
                btnLabel: t("vaultManager.create"),
                onClick: () => { void onOpenVault(); },
                primary: false,
              },
              {
                title: t("vaultManager.systemSettings"),
                desc: t("vaultManager.systemSettingsDesc"),
                btnLabel: t("vaultManager.settings"),
                onClick: onOpenSettings,
                primary: false,
              },
              {
                title: t("vaultManager.truthBoard"),
                desc: t("vaultManager.truthBoardDesc"),
                btnLabel: t("vaultManager.open"),
                onClick: onOpenTruth,
                primary: false,
              },
            ].map(card => (
              <div key={card.title} className="flex justify-between items-center p-4 rounded-xl transition-colors duration-150
                hover:bg-[var(--sidebar-hover)] bg-[var(--subtle-surface)] border-[0.5px] border-[var(--separator-light)]">
                <div>
                  <p className="text-[14px] font-medium text-[var(--text-secondary)]">{card.title}</p>
                  <p className="text-[12px] mt-1 text-[var(--text-quaternary)]">{card.desc}</p>
                </div>
                <button
                  type="button"
                  onClick={card.onClick}
                  className={`px-5 py-1.5 rounded-lg text-[13px] font-medium cursor-pointer transition-colors duration-150 shrink-0 ml-4 ${
                    card.primary
                      ? "bg-[var(--accent)] text-white shadow-[0_1px_4px_rgba(10,132,255,0.25)]"
                      : "hover:bg-[var(--surface-3)] bg-[var(--subtle-surface-strong)] text-[var(--text-secondary)] border-[0.5px] border-[var(--separator-light)]"
                  }`}
                >
                  {card.btnLabel}
                </button>
              </div>
            ))}
          </div>
        </div>
      </main>
      </div>

      {contextMenu && createPortal(
        <>
          <div
            className="fixed inset-0 z-[9999]"
            style={{ background: "transparent" }}
            onPointerDown={() => setContextMenu(null)}
            onContextMenu={e => {
              e.preventDefault();
              setContextMenu(null);
            }}
          />
          <div
            ref={contextMenuRef}
            className="fixed z-[10000] w-[196px] rounded-lg p-1"
            style={{
              left: `${Math.max(8, Math.min(contextMenu.x, window.innerWidth - RECENT_MENU_WIDTH))}px`,
              top: `${Math.max(8, Math.min(contextMenu.y, window.innerHeight - RECENT_MENU_MAX_HEIGHT))}px`,
              background: "var(--menu-bg)",
              border: "1px solid var(--separator-light)",
              boxShadow: "0 10px 28px rgba(0,0,0,0.35)",
            }}
            onClick={e => e.stopPropagation()}
          >
            <button
              type="button"
              className="w-full text-left px-2.5 py-1.5 rounded-md text-[12px] leading-5 transition-colors hover:bg-[var(--menu-hover)]"
              style={{ color: "var(--text-secondary)" }}
              onClick={() => {
                void onOpenRecent(contextMenu.vault.path);
                setContextMenu(null);
              }}
            >
              {t("vaultManager.openVault")}
            </button>
            <div className="my-1 h-px" style={{ background: "var(--separator-light)" }} />
            <button
              type="button"
              className="w-full text-left px-2.5 py-1.5 rounded-md text-[12px] leading-5 transition-colors hover:bg-[var(--menu-hover)]"
              style={{ color: "rgba(255,75,75,0.95)" }}
              onClick={() => {
                handleRemoveRecent(contextMenu.vault);
              }}
            >
              {t("vaultManager.removeFromRecent")}
            </button>
          </div>
        </>,
        document.body
      )}
    </>
  );
}
