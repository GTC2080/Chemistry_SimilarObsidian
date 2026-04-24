import { useT } from "../../i18n";
import type { SettingsState } from "./settingsTypes";
import { inputClass, labelClass, hintClass } from "./settingsShared";
import type { SettingsUpdate, TestResult } from "./settingsShared";

export default function AiSettingsPanel({
  settings,
  onUpdate,
  testing,
  testResult,
  onTest,
}: {
  settings: SettingsState;
  onUpdate: SettingsUpdate;
  testing: boolean;
  testResult: TestResult | null;
  onTest: () => void;
}) {
  const t = useT();
  return (
    <div className="space-y-6 max-w-lg">
      <div>
        <p className="text-xs font-semibold text-[var(--text-tertiary)] uppercase tracking-wider mb-4">{t("settings.chatModel")}</p>
        <div className="space-y-4">
          <div>
            <label className={labelClass}>{t("settings.apiKey")}</label>
            <input type="password" className={inputClass} value={settings.chatApiKey} onChange={e => onUpdate("chatApiKey", e.target.value)} placeholder="sk-..." />
          </div>
          <div>
            <label className={labelClass}>{t("settings.baseUrl")}</label>
            <input type="text" className={inputClass} value={settings.chatBaseUrl} onChange={e => onUpdate("chatBaseUrl", e.target.value)} placeholder="https://api.openai.com/v1" />
          </div>
          <div>
            <label className={labelClass}>{t("settings.modelName")}</label>
            <input type="text" className={inputClass} value={settings.chatModel} onChange={e => onUpdate("chatModel", e.target.value)} placeholder="gpt-4o-mini" />
          </div>
        </div>
      </div>

      <hr className="border-[var(--separator-light)]" />

      <div>
        <p className="text-xs font-semibold text-[var(--text-tertiary)] uppercase tracking-wider mb-1">{t("settings.embeddingModel")}</p>
        <p className={hintClass + " mb-4"}>{t("settings.embeddingFallbackHint")}</p>
        <div className="space-y-4">
          <div>
            <label className={labelClass}>{t("settings.apiKey")}</label>
            <input type="password" className={inputClass} value={settings.embeddingApiKey} onChange={e => onUpdate("embeddingApiKey", e.target.value)} placeholder={t("settings.embeddingApiKeyPlaceholder")} />
          </div>
          <div>
            <label className={labelClass}>{t("settings.baseUrl")}</label>
            <input type="text" className={inputClass} value={settings.embeddingBaseUrl} onChange={e => onUpdate("embeddingBaseUrl", e.target.value)} placeholder={t("settings.embeddingBaseUrlPlaceholder")} />
          </div>
          <div>
            <label className={labelClass}>{t("settings.modelName")}</label>
            <input type="text" className={inputClass} value={settings.embeddingModel} onChange={e => onUpdate("embeddingModel", e.target.value)} placeholder="text-embedding-3-small" />
          </div>
        </div>
      </div>

      <div className="flex items-center gap-3">
        <button
          type="button"
          onClick={onTest}
          disabled={testing || !settings.chatApiKey}
          className="px-4 py-2 rounded-md text-sm font-medium cursor-pointer transition-colors
            bg-[rgba(255,255,255,0.04)] border border-[var(--separator-light)] text-[var(--text-secondary)] hover:bg-[rgba(255,255,255,0.08)] hover:text-[var(--text-primary)]
            disabled:opacity-40 disabled:cursor-not-allowed"
        >
          {testing ? t("settings.testing") : t("settings.testConnection")}
        </button>
        {testResult && (
          <span className={`text-xs flex items-center gap-1.5 ${testResult.ok ? "text-emerald-400" : "text-red-400"}`}>
            <svg className="w-3.5 h-3.5 shrink-0" viewBox="0 0 24 24" fill="none" stroke="currentColor" strokeWidth="2" strokeLinecap="round" strokeLinejoin="round">
              {testResult.ok
                ? <polyline points="20 6 9 17 4 12" />
                : <><circle cx="12" cy="12" r="10" /><line x1="15" y1="9" x2="9" y2="15" /><line x1="9" y1="9" x2="15" y2="15" /></>
              }
            </svg>
            {testResult.msg}
          </span>
        )}
      </div>

      <hr className="border-[var(--separator-light)]" />

      <div>
        <p className="text-xs font-semibold text-[var(--text-tertiary)] uppercase tracking-wider mb-4">{t("settings.advanced")}</p>
        <div className="space-y-5">
          <div>
            <div className="flex items-center justify-between mb-1.5">
              <label className="text-sm font-medium text-[var(--text-secondary)]">Temperature</label>
              <span className="text-xs text-[var(--text-tertiary)] tabular-nums">{settings.temperature.toFixed(1)}</span>
            </div>
            <input
              type="range"
              min="0"
              max="2"
              step="0.1"
              value={settings.temperature}
              onChange={e => onUpdate("temperature", parseFloat(e.target.value))}
              className="w-full h-1 rounded-full appearance-none cursor-pointer bg-[rgba(255,255,255,0.16)]
                [&::-webkit-slider-thumb]:appearance-none [&::-webkit-slider-thumb]:w-3.5 [&::-webkit-slider-thumb]:h-3.5
                [&::-webkit-slider-thumb]:rounded-full [&::-webkit-slider-thumb]:bg-[var(--accent)] [&::-webkit-slider-thumb]:shadow-[0_0_10px_rgba(10,132,255,0.45)]
                [&::-webkit-slider-thumb]:cursor-pointer"
            />
            <p className={hintClass}>{t("settings.temperatureHint")}</p>
          </div>
          <div>
            <label className={labelClass}>{t("settings.systemPrompt")}</label>
            <textarea className={inputClass + " min-h-[100px] resize-y"} value={settings.systemPrompt} onChange={e => onUpdate("systemPrompt", e.target.value)} placeholder={t("settings.systemPromptPlaceholder")} />
            <p className={hintClass}>{t("settings.systemPromptHint")}</p>
          </div>
        </div>
      </div>
    </div>
  );
}
