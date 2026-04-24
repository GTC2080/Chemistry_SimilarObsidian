import { useState, useEffect, useCallback } from "react";
import { invoke } from "@tauri-apps/api/core";
import { useT } from "../i18n";
import { settingsStore, persistStoreValues } from "../utils/settingsStore";
import type { SettingsUpdate } from "../components/settings/SettingsPanels";
import {
  DEFAULT_SETTINGS,
  DEFAULT_VISIBLE_ACTIVITY_BAR,
  applyRuntimeSettings,
  normalizeDisciplineProfile,
  normalizeTheme,
  toRuntimeSettings,
  type ActivityBarItemId,
  type RuntimeSettings,
  type SettingsState,
  type SettingsTab,
} from "../components/settings/settingsTypes";

type OperationResult = { ok: boolean; msg: string } | null;

function buildFullSettingsPayload(settings: SettingsState): Record<string, unknown> {
  return {
    uiLanguage: settings.uiLanguage,
    theme: settings.theme,
    aiApiKey: settings.chatApiKey,
    aiBaseUrl: settings.chatBaseUrl,
    chatModel: settings.chatModel,
    embeddingApiKey: settings.embeddingApiKey,
    embeddingBaseUrl: settings.embeddingBaseUrl,
    embeddingModel: settings.embeddingModel,
    temperature: settings.temperature,
    systemPrompt: settings.systemPrompt,
    fontFamily: settings.fontFamily,
    enableScientific: settings.enableScientific,
    ignoredFolders: settings.ignoredFolders,
    activeDiscipline: settings.activeDiscipline,
    visibleActivityBarItems: settings.visibleActivityBarItems,
  };
}

function buildAiSettingsPayload(settings: SettingsState): Record<string, unknown> {
  return {
    aiApiKey: settings.chatApiKey,
    aiBaseUrl: settings.chatBaseUrl,
    chatModel: settings.chatModel,
    embeddingApiKey: settings.embeddingApiKey,
    embeddingBaseUrl: settings.embeddingBaseUrl,
    embeddingModel: settings.embeddingModel,
  };
}

async function loadSettingsState(): Promise<SettingsState> {
  const [
    uiLanguageRaw,
    themeRaw,
    chatApiKeyRaw,
    chatBaseUrlRaw,
    chatModelRaw,
    embeddingApiKeyRaw,
    embeddingBaseUrlRaw,
    embeddingModelRaw,
    temperatureRaw,
    systemPromptRaw,
    fontFamilyRaw,
    enableScientificRaw,
    ignoredFoldersRaw,
    activeDisciplineRaw,
    visibleActivityBarRaw,
  ] = await Promise.all([
    settingsStore.get("uiLanguage"),
    settingsStore.get("theme"),
    settingsStore.get("aiApiKey"),
    settingsStore.get("aiBaseUrl"),
    settingsStore.get("chatModel"),
    settingsStore.get("embeddingApiKey"),
    settingsStore.get("embeddingBaseUrl"),
    settingsStore.get("embeddingModel"),
    settingsStore.get("temperature"),
    settingsStore.get("systemPrompt"),
    settingsStore.get("fontFamily"),
    settingsStore.get("enableScientific"),
    settingsStore.get("ignoredFolders"),
    settingsStore.get("activeDiscipline"),
    settingsStore.get("visibleActivityBarItems"),
  ]);

  return {
    uiLanguage: (uiLanguageRaw as string) || DEFAULT_SETTINGS.uiLanguage,
    theme: normalizeTheme(themeRaw),
    chatApiKey: (chatApiKeyRaw as string) || "",
    chatBaseUrl: (chatBaseUrlRaw as string) || DEFAULT_SETTINGS.chatBaseUrl,
    chatModel: (chatModelRaw as string) || DEFAULT_SETTINGS.chatModel,
    embeddingApiKey: (embeddingApiKeyRaw as string) || "",
    embeddingBaseUrl: (embeddingBaseUrlRaw as string) || "",
    embeddingModel: (embeddingModelRaw as string) || DEFAULT_SETTINGS.embeddingModel,
    temperature: (temperatureRaw as number) ?? DEFAULT_SETTINGS.temperature,
    systemPrompt: (systemPromptRaw as string) || "",
    fontFamily: (fontFamilyRaw as string) || DEFAULT_SETTINGS.fontFamily,
    enableScientific: (enableScientificRaw as boolean) ?? DEFAULT_SETTINGS.enableScientific,
    ignoredFolders: (ignoredFoldersRaw as string) || DEFAULT_SETTINGS.ignoredFolders,
    activeDiscipline: normalizeDisciplineProfile(activeDisciplineRaw),
    visibleActivityBarItems: Array.isArray(visibleActivityBarRaw)
      ? (visibleActivityBarRaw as ActivityBarItemId[])
      : DEFAULT_VISIBLE_ACTIVITY_BAR,
  };
}

interface UseSettingsModalArgs {
  open: boolean;
  onClose: () => void;
  onSettingsApplied?: (settings: RuntimeSettings) => void;
}

export function useSettingsModal({ open, onClose, onSettingsApplied }: UseSettingsModalArgs) {
  const t = useT();
  const [activeTab, setActiveTab] = useState<SettingsTab>("general");
  const [settings, setSettings] = useState<SettingsState>(DEFAULT_SETTINGS);
  const [saving, setSaving] = useState(false);
  const [testing, setTesting] = useState(false);
  const [rebuilding, setRebuilding] = useState(false);
  const [testResult, setTestResult] = useState<OperationResult>(null);
  const [rebuildResult, setRebuildResult] = useState<OperationResult>(null);

  useEffect(() => {
    if (!open) return;
    setTestResult(null);
    setRebuildResult(null);
    (async () => {
      try {
        setSettings(await loadSettingsState());
      } catch {
        setSettings(DEFAULT_SETTINGS);
      }
    })();
  }, [open]);

  const updateSetting: SettingsUpdate = useCallback((key, value) => {
    setSettings(prev => ({ ...prev, [key]: value }));
  }, []);

  const handleSave = useCallback(async () => {
    setSaving(true);
    try {
      await persistStoreValues(buildFullSettingsPayload(settings));
      const runtime = toRuntimeSettings(settings);
      applyRuntimeSettings(runtime);
      onSettingsApplied?.(runtime);
      onClose();
    } catch (error) {
      console.error(t("settings.saveFailed"), error);
    } finally {
      setSaving(false);
    }
  }, [settings, onClose, onSettingsApplied]);

  const handleTest = useCallback(async () => {
    setTesting(true);
    setTestResult(null);
    try {
      await persistStoreValues(buildAiSettingsPayload(settings));
      const msg = await invoke<string>("test_ai_connection");
      setTestResult({ ok: true, msg });
    } catch (error) {
      setTestResult({ ok: false, msg: error instanceof Error ? error.message : String(error) });
    } finally {
      setTesting(false);
    }
  }, [settings]);

  const handleRerunOnboarding = useCallback(async () => {
    await persistStoreValues({ onboardingCompleted: false });
    onClose();
    window.location.reload();
  }, [onClose]);

  const handleRebuildVectors = useCallback(async () => {
    const ok = window.confirm(t("settings.rebuildConfirm"));
    if (!ok) return;
    setRebuilding(true);
    setRebuildResult(null);
    try {
      const count = await invoke<number>("rebuild_vector_index");
      setRebuildResult({ ok: true, msg: t("settings.rebuildComplete", { count }) });
    } catch (error) {
      setRebuildResult({ ok: false, msg: error instanceof Error ? error.message : String(error) });
    } finally {
      setRebuilding(false);
    }
  }, []);

  return {
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
  };
}
