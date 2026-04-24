import { useT } from "../../i18n";
import { ACTIVITY_BAR_ITEMS, type ActivityBarItemId } from "./settingsTypes";
import type { SettingsState } from "./settingsTypes";
import { labelClass, hintClass } from "./settingsShared";
import type { SettingsUpdate } from "./settingsShared";

export default function FeaturesSettingsPanel({
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
        <label className={labelClass}>{t("settings.activityBarFeatures")}</label>
        <p className={hintClass + " mb-4"}>{t("settings.activityBarHint")}</p>
        <div className="space-y-2">
          {ACTIVITY_BAR_ITEMS.map(item => {
            const visible = settings.visibleActivityBarItems.includes(item.id);
            return (
              <div key={item.id} className="flex items-center justify-between py-1">
                <span className="text-sm text-[var(--text-secondary)]">{t(item.labelKey)}</span>
                <button
                  type="button"
                  onClick={() => {
                    const next = visible
                      ? settings.visibleActivityBarItems.filter((id: ActivityBarItemId) => id !== item.id)
                      : [...settings.visibleActivityBarItems, item.id];
                    onUpdate("visibleActivityBarItems", next);
                  }}
                  className="relative w-11 h-6 rounded-full transition-colors duration-200 cursor-pointer flex-shrink-0"
                  style={{ background: visible ? "var(--accent)" : "var(--separator)" }}
                >
                  <span className={`absolute top-0.5 left-0.5 w-5 h-5 rounded-full bg-white shadow transition-transform duration-200 ${
                    visible ? "translate-x-5" : "translate-x-0"
                  }`} />
                </button>
              </div>
            );
          })}
        </div>
      </div>
    </div>
  );
}
