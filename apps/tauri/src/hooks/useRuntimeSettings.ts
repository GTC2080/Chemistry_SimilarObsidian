import { useEffect, useState } from "react";
import {
  DEFAULT_RUNTIME_SETTINGS,
  DEFAULT_VISIBLE_ACTIVITY_BAR,
  normalizeDisciplineProfile,
  normalizeTheme,
  type ActivityBarItemId,
  type RuntimeSettings,
} from "../components/settings/settingsTypes";
import { settingsStore } from "../utils/settingsStore";

export function useRuntimeSettings() {
  const [runtimeSettings, setRuntimeSettings] = useState<RuntimeSettings>(DEFAULT_RUNTIME_SETTINGS);
  const [loaded, setLoaded] = useState(false);
  const [onboardingCompleted, setOnboardingCompleted] = useState(false);

  useEffect(() => {
    (async () => {
      try {
        const [
          uiLanguageRaw,
          themeRaw,
          fontFamilyRaw,
          enableScientificRaw,
          ignoredFoldersRaw,
          activeDisciplineRaw,
          onboardingCompletedRaw,
          visibleActivityBarRaw,
        ] = await Promise.all([
          settingsStore.get("uiLanguage"),
          settingsStore.get("theme"),
          settingsStore.get("fontFamily"),
          settingsStore.get("enableScientific"),
          settingsStore.get("ignoredFolders"),
          settingsStore.get("activeDiscipline"),
          settingsStore.get("onboardingCompleted"),
          settingsStore.get("visibleActivityBarItems"),
        ]);
        const uiLanguage = (uiLanguageRaw as string) || DEFAULT_RUNTIME_SETTINGS.uiLanguage;
        const theme = normalizeTheme(themeRaw);
        const fontFamily = (fontFamilyRaw as string) || DEFAULT_RUNTIME_SETTINGS.fontFamily;
        const enableScientific = (enableScientificRaw as boolean) ?? DEFAULT_RUNTIME_SETTINGS.enableScientific;
        const ignoredFolders = (ignoredFoldersRaw as string) || DEFAULT_RUNTIME_SETTINGS.ignoredFolders;
        const activeDiscipline = normalizeDisciplineProfile(activeDisciplineRaw);
        const visibleActivityBarItems = Array.isArray(visibleActivityBarRaw)
          ? (visibleActivityBarRaw as ActivityBarItemId[])
          : DEFAULT_VISIBLE_ACTIVITY_BAR;
        const parsedSettings: RuntimeSettings = { uiLanguage, theme, fontFamily, enableScientific, ignoredFolders, activeDiscipline, visibleActivityBarItems };
        setRuntimeSettings(parsedSettings);
        setOnboardingCompleted((onboardingCompletedRaw as boolean) === true);
        setLoaded(true);
      } catch {
        // ignore settings load error
        setLoaded(true);
      }
    })();
  }, []);

  useEffect(() => {
    document.documentElement.lang = runtimeSettings.uiLanguage || "zh-CN";
    document.documentElement.setAttribute("data-theme", runtimeSettings.theme || "dark");
  }, [runtimeSettings.uiLanguage, runtimeSettings.theme]);

  return { runtimeSettings, setRuntimeSettings, loaded, onboardingCompleted, setOnboardingCompleted };
}
