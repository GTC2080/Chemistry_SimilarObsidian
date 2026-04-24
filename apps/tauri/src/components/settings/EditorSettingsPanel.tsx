import { useT } from "../../i18n";
import type { SettingsState } from "./settingsTypes";
import { inputClass, labelClass, hintClass } from "./settingsShared";
import type { SettingsUpdate } from "./settingsShared";

export default function EditorSettingsPanel({
  settings,
  onUpdate,
}: {
  settings: SettingsState;
  onUpdate: SettingsUpdate;
}) {
  const t = useT();
  return (
    <div className="space-y-6 max-w-lg">
      <div>
        <label className={labelClass}>{t("settings.font")}</label>
        <input
          type="text"
          className={inputClass}
          value={settings.fontFamily}
          onChange={e => onUpdate("fontFamily", e.target.value)}
          placeholder="System Default"
        />
        <p className={hintClass}>{t("settings.fontHint")}</p>
      </div>

      <div className="flex items-center justify-between">
        <div>
          <label className={labelClass}>{t("settings.scientific")}</label>
          <p className={hintClass + " mt-0"}>{t("settings.scientificHint")}</p>
        </div>
        <button
          type="button"
          onClick={() => onUpdate("enableScientific", !settings.enableScientific)}
          className="relative w-11 h-6 rounded-full transition-colors duration-200 cursor-pointer flex-shrink-0"
          style={{ background: settings.enableScientific ? "var(--accent)" : "var(--separator)" }}
        >
          <span className={`absolute top-0.5 left-0.5 w-5 h-5 rounded-full bg-white shadow transition-transform duration-200 ${
            settings.enableScientific ? "translate-x-5" : "translate-x-0"
          }`}
          />
        </button>
      </div>
    </div>
  );
}
