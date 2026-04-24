import { lazy, Suspense, useCallback, useEffect, useState } from "react";
import { open } from "@tauri-apps/plugin-dialog";
import { useRuntimeSettings } from "./hooks/useRuntimeSettings";
import { useRecentVaults } from "./hooks/useRecentVaults";
import type { RuntimeSettings } from "./components/settings/settingsTypes";
import { useLazyModalReady } from "./hooks/useLazyModalReady";
import { useWindowControls } from "./hooks/useWindowControls";
import type { TruthState } from "./models/truth_system";
import { LanguageProvider } from "./i18n";
import AppTitleBar from "./components/app/AppTitleBar";
import VaultManagerView from "./components/app/VaultManagerView";
import LaunchSplash from "./components/app/LaunchSplash";
import ErrorBoundary from "./components/ErrorBoundary";

const WorkspaceRuntime = lazy(() => import("./components/app/WorkspaceRuntime"));
const SettingsModal = lazy(() => import("./components/SettingsModal"));
const TruthDashboard = lazy(() => import("./components/TruthDashboard"));
const OnboardingWizard = lazy(() => import("./components/onboarding/OnboardingWizard"));

const DEFAULT_TRUTH_STATE: TruthState = {
  level: 1,
  totalExp: 0,
  nextLevelExp: 100,
  attributes: { science: 1, engineering: 1, creation: 1, finance: 1 },
  attributeExp: { science: 0, engineering: 0, creation: 0, finance: 0 },
  lastSettlement: Date.now(),
};

function App() {
  const { runtimeSettings, setRuntimeSettings, loaded, onboardingCompleted, setOnboardingCompleted } = useRuntimeSettings();
  const { recentVaults, saveToRecent, removeFromRecent } = useRecentVaults();
  const [workspaceVaultPath, setWorkspaceVaultPath] = useState("");
  const [managerSettingsOpen, setManagerSettingsOpen] = useState(false);
  const [managerTruthOpen, setManagerTruthOpen] = useState(false);
  const [truthSnapshot, setTruthSnapshot] = useState<TruthState>(DEFAULT_TRUTH_STATE);
  const windowControls = useWindowControls();
  const [activeLanguage, setActiveLanguage] = useState(runtimeSettings.uiLanguage);

  useEffect(() => {
    setActiveLanguage(runtimeSettings.uiLanguage);
  }, [runtimeSettings.uiLanguage]);

  // App 已挂载 — 淡出并移除纯 HTML 启动画面
  useEffect(() => {
    const el = document.getElementById("pre-splash");
    if (!el) return;
    el.classList.add("fade-out");
    const onDone = () => el.remove();
    el.addEventListener("transitionend", onDone, { once: true });
    // 兜底：如果 transitionend 未触发（例如动画被跳过），300ms 后强制移除
    const fallback = setTimeout(onDone, 400);
    return () => clearTimeout(fallback);
  }, []);

  const managerSettingsReady = useLazyModalReady(managerSettingsOpen && !workspaceVaultPath);
  const managerTruthReady = useLazyModalReady(managerTruthOpen && !workspaceVaultPath);

  const handleOpenVault = async () => {
    try {
      const selected = await open({ directory: true, multiple: false });
      if (!selected) return;
      setWorkspaceVaultPath(selected);
    } catch (error) {
      console.error("打开知识库失败:", error);
    }
  };

  const handleOpenRecent = (path: string) => {
    setWorkspaceVaultPath(path);
  };

  const handleOnboardingComplete = useCallback((settings: RuntimeSettings) => {
    setRuntimeSettings(settings);
    setOnboardingCompleted(true);
  }, [setRuntimeSettings, setOnboardingCompleted]);

  return (
    <LanguageProvider language={activeLanguage}>
    <div className="h-screen w-screen workspace-canvas">
      <div className="h-full w-full overflow-hidden flex flex-col">

        {/* ========== Title Bar ========== */}
        <AppTitleBar
          onBackgroundMouseDown={windowControls.onBackgroundMouseDown}
          onBackgroundDoubleClick={windowControls.onBackgroundDoubleClick}
          onMinimize={windowControls.onMinimize}
          onToggleMaximize={windowControls.onToggleMaximize}
          onClose={windowControls.onClose}
        />

        {/* ========== Main Content ========== */}
        {!loaded ? (
          <LaunchSplash />
        ) : !onboardingCompleted ? (
          <ErrorBoundary>
            <Suspense fallback={<LaunchSplash />}>
              <OnboardingWizard onComplete={handleOnboardingComplete} onLanguageChange={setActiveLanguage} />
            </Suspense>
          </ErrorBoundary>
        ) : !workspaceVaultPath ? (
          <div className="flex flex-1 min-h-0">
            <VaultManagerView
              recentVaults={recentVaults}
              onOpenRecent={handleOpenRecent}
              onRemoveRecent={removeFromRecent}
              onOpenVault={handleOpenVault}
              onOpenSettings={() => setManagerSettingsOpen(true)}
              onOpenTruth={() => setManagerTruthOpen(true)}
            />
          </div>
        ) : (
          <ErrorBoundary>
            <Suspense fallback={<LaunchSplash />}>
              <WorkspaceRuntime
                key={workspaceVaultPath}
                initialVaultPath={workspaceVaultPath}
                runtimeSettings={runtimeSettings}
                onRuntimeSettingsApplied={setRuntimeSettings}
                onSaveToRecent={saveToRecent}
                onExitWorkspace={() => setWorkspaceVaultPath("")}
                onTruthStateSnapshot={setTruthSnapshot}
              />
            </Suspense>
          </ErrorBoundary>
        )}
      </div>

      {onboardingCompleted && !workspaceVaultPath && managerSettingsReady && (
        <ErrorBoundary>
          <Suspense fallback={null}>
            <SettingsModal
              open={managerSettingsOpen}
              onClose={() => setManagerSettingsOpen(false)}
              onSettingsApplied={setRuntimeSettings}
            />
          </Suspense>
        </ErrorBoundary>
      )}
      {onboardingCompleted && !workspaceVaultPath && managerTruthReady && (
        <ErrorBoundary>
          <Suspense fallback={null}>
            <TruthDashboard
              open={managerTruthOpen}
              onClose={() => setManagerTruthOpen(false)}
              state={truthSnapshot}
            />
          </Suspense>
        </ErrorBoundary>
      )}
    </div>
    </LanguageProvider>
  );
}

export default App;
