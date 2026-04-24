export type DisciplineProfile = "chemistry";

export type ActivityBarItemId = "search" | "graph" | "ai" | "chemdraw" | "kinetics" | "timeline";

export const ACTIVITY_BAR_ITEMS: { id: ActivityBarItemId; labelKey: string }[] = [
  { id: "search", labelKey: "activityBar.search" },
  { id: "graph", labelKey: "activityBar.graph" },
  { id: "ai", labelKey: "activityBar.ai" },
  { id: "chemdraw", labelKey: "activityBar.chemdraw" },
  { id: "kinetics", labelKey: "activityBar.kinetics" },
  { id: "timeline", labelKey: "activityBar.timeline" },
];

export const DEFAULT_VISIBLE_ACTIVITY_BAR: ActivityBarItemId[] = ["search", "graph", "ai", "chemdraw", "kinetics", "timeline"];

export interface RuntimeSettings {
  uiLanguage: string;
  theme: "dark" | "light";
  fontFamily: string;
  enableScientific: boolean;
  ignoredFolders: string;
  activeDiscipline: DisciplineProfile;
  visibleActivityBarItems: ActivityBarItemId[];
}

export interface SettingsState extends RuntimeSettings {
  chatApiKey: string;
  chatBaseUrl: string;
  chatModel: string;
  embeddingApiKey: string;
  embeddingBaseUrl: string;
  embeddingModel: string;
  temperature: number;
  systemPrompt: string;
}

export type SettingsTab = "general" | "features" | "editor" | "ai" | "vault";

export const SETTINGS_STORE_NAME = "settings.json";
export const DISCIPLINE_PROFILES: DisciplineProfile[] = ["chemistry"];

export const DEFAULT_SETTINGS: SettingsState = {
  uiLanguage: "zh-CN",
  theme: "dark",
  chatApiKey: "",
  chatBaseUrl: "https://api.openai.com/v1",
  chatModel: "gpt-4o-mini",
  embeddingApiKey: "",
  embeddingBaseUrl: "",
  embeddingModel: "text-embedding-3-small",
  temperature: 0.7,
  systemPrompt: "",
  fontFamily: "System Default",
  enableScientific: false,
  ignoredFolders: "node_modules, .git",
  activeDiscipline: "chemistry",
  visibleActivityBarItems: DEFAULT_VISIBLE_ACTIVITY_BAR,
};

export const DEFAULT_RUNTIME_SETTINGS: RuntimeSettings = {
  uiLanguage: DEFAULT_SETTINGS.uiLanguage,
  theme: DEFAULT_SETTINGS.theme,
  fontFamily: DEFAULT_SETTINGS.fontFamily,
  enableScientific: DEFAULT_SETTINGS.enableScientific,
  ignoredFolders: DEFAULT_SETTINGS.ignoredFolders,
  activeDiscipline: DEFAULT_SETTINGS.activeDiscipline,
  visibleActivityBarItems: DEFAULT_VISIBLE_ACTIVITY_BAR,
};

export function normalizeTheme(value: unknown): RuntimeSettings["theme"] {
  return value === "light" || value === "dark" ? value : DEFAULT_SETTINGS.theme;
}

export function normalizeDisciplineProfile(value: unknown): DisciplineProfile {
  return DISCIPLINE_PROFILES.includes(value as DisciplineProfile)
    ? (value as DisciplineProfile)
    : DEFAULT_SETTINGS.activeDiscipline;
}

export function toRuntimeSettings(settings: SettingsState): RuntimeSettings {
  return {
    uiLanguage: settings.uiLanguage,
    theme: settings.theme,
    fontFamily: settings.fontFamily,
    enableScientific: settings.enableScientific,
    ignoredFolders: settings.ignoredFolders,
    activeDiscipline: settings.activeDiscipline,
    visibleActivityBarItems: settings.visibleActivityBarItems,
  };
}

export function applyRuntimeSettings(settings: Pick<RuntimeSettings, "uiLanguage" | "theme">) {
  document.documentElement.lang = settings.uiLanguage || DEFAULT_SETTINGS.uiLanguage;
  document.documentElement.setAttribute("data-theme", settings.theme || DEFAULT_SETTINGS.theme);
}
