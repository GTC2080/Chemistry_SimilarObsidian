import { useAppVersion } from "../hooks/useAppVersion";
import { useSettingsModal } from "../hooks/useSettingsModal";
import { useT } from "../i18n";
import {
  AiSettingsPanel,
  EditorSettingsPanel,
  FeaturesSettingsPanel,
  GeneralSettingsPanel,
  SETTINGS_TABS,
  VaultSettingsPanel,
} from "./settings/SettingsPanels";
import type { DisciplineProfile, RuntimeSettings } from "./settings/settingsTypes";

export type { DisciplineProfile, RuntimeSettings };

interface SettingsModalProps {
  open: boolean;
  onClose: () => void;
  onSettingsApplied?: (settings: RuntimeSettings) => void;
}

export default function SettingsModal({ open, onClose, onSettingsApplied }: SettingsModalProps) {
  const t = useT();
  const appVersion = useAppVersion();
  const {
    activeTab,
    setActiveTab,
    settings,
    saving,
    testing,
    rebuilding,
    testResult,
    rebuildResult,
    updateSetting,
    handleSave,
    handleTest,
    handleRerunOnboarding,
    handleRebuildVectors,
  } = useSettingsModal({ open, onClose, onSettingsApplied });

  if (!open) return null;

  return (
    <div
      className="fixed inset-0 z-50 flex items-center justify-center"
      style={{
        background: "radial-gradient(1200px 700px at 12% 10%, rgba(10,132,255,0.08) 0%, rgba(0,0,0,0) 40%), rgba(0,0,0,0.55)",
        backdropFilter: "blur(24px)",
      }}
      onClick={event => {
        if (event.target === event.currentTarget) onClose();
      }}
    >
      <div className="w-full max-w-4xl h-[70vh] min-h-[500px] rounded-2xl overflow-hidden flex animate-modal-in glass-elevated">
        <div className="w-52 bg-[rgba(255,255,255,0.02)] border-r border-[var(--separator-light)] flex flex-col">
          <div className="px-5 pt-5 pb-3">
            <h2 className="text-[15px] font-semibold text-[var(--text-primary)] tracking-[-0.01em]">{t("settings.title")}</h2>
          </div>
          <nav className="flex-1 py-1">
            {SETTINGS_TABS.map(tab => (
              <button
                key={tab.key}
                type="button"
                onClick={() => setActiveTab(tab.key)}
                className={`w-full flex items-center gap-2.5 px-4 py-2 mx-2 mt-0.5 rounded-md text-sm cursor-pointer transition-colors
                  ${activeTab === tab.key
                    ? "bg-[var(--accent-soft)] text-[var(--text-primary)] border border-[rgba(10,132,255,0.35)] font-medium"
                    : "text-[var(--text-tertiary)] hover:bg-[rgba(255,255,255,0.05)] hover:text-[var(--text-secondary)] border border-transparent"
                  }`}
                style={{ width: "calc(100% - 16px)" }}
              >
                {tab.icon}
                {t(tab.labelKey)}
              </button>
            ))}
          </nav>
          <div className="px-5 pb-4 text-[11px] text-[var(--text-quaternary)]">v{appVersion}</div>
        </div>

        <div className="flex-1 flex flex-col min-w-0">
          <div className="flex items-center justify-between px-8 pt-6 pb-4 border-b border-[var(--separator-light)]">
            <h3 className="text-[14px] font-semibold text-[var(--text-primary)]">
              {(() => { const tab = SETTINGS_TABS.find(tab => tab.key === activeTab); return tab ? t(tab.labelKey) : ""; })()}
            </h3>
            <button
              type="button"
              onClick={onClose}
              className="w-7 h-7 rounded-full flex items-center justify-center transition-all duration-150 cursor-pointer hover:bg-[rgba(255,255,255,0.08)] active:scale-90 text-[var(--text-quaternary)] hover:text-[var(--text-secondary)]"
              aria-label={t("settings.close")}
            >
              <svg className="w-3 h-3" viewBox="0 0 24 24" fill="none" stroke="currentColor" strokeWidth="2.5" strokeLinecap="round" strokeLinejoin="round">
                <line x1="18" y1="6" x2="6" y2="18" />
                <line x1="6" y1="6" x2="18" y2="18" />
              </svg>
            </button>
          </div>

          <div className="flex-1 overflow-y-auto p-8">
            {activeTab === "general" && (
              <GeneralSettingsPanel
                settings={settings}
                onUpdate={updateSetting}
                onRerunOnboarding={() => { void handleRerunOnboarding(); }}
              />
            )}
            {activeTab === "features" && <FeaturesSettingsPanel settings={settings} onUpdate={updateSetting} />}
            {activeTab === "editor" && <EditorSettingsPanel settings={settings} onUpdate={updateSetting} />}
            {activeTab === "ai" && (
              <AiSettingsPanel
                settings={settings}
                onUpdate={updateSetting}
                testing={testing}
                testResult={testResult}
                onTest={() => { void handleTest(); }}
              />
            )}
            {activeTab === "vault" && (
              <VaultSettingsPanel
                settings={settings}
                onUpdate={updateSetting}
                rebuilding={rebuilding}
                rebuildResult={rebuildResult}
                onRebuildVectors={() => { void handleRebuildVectors(); }}
              />
            )}
          </div>

          <div className="px-8 py-4 border-t border-[var(--separator-light)] flex items-center justify-end gap-3 bg-[rgba(255,255,255,0.02)]">
            <button
              type="button"
              onClick={onClose}
              className="px-4 py-2 rounded-md text-sm cursor-pointer text-[var(--text-tertiary)] hover:text-[var(--text-primary)] hover:bg-[rgba(255,255,255,0.08)] transition-colors"
            >
              {t("settings.cancel")}
            </button>
            <button
              type="button"
              onClick={() => { void handleSave(); }}
              disabled={saving}
              className="px-5 py-2 rounded-md text-sm font-medium cursor-pointer disabled:opacity-50 transition-all hover:brightness-110"
              style={{
                background: "linear-gradient(135deg, #0A84FF 0%, #0066D6 100%)",
                color: "#fff",
                boxShadow: "0 6px 18px rgba(10,132,255,0.28), inset 0 1px 0 rgba(255,255,255,0.18)",
              }}
            >
              {saving ? t("settings.saving") : t("settings.save")}
            </button>
          </div>
        </div>
      </div>
    </div>
  );
}
