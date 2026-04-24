import { useT } from "../../i18n";
import type { SettingsState } from "./settingsTypes";
import { inputClass, labelClass, hintClass } from "./settingsShared";
import type { SettingsUpdate, TestResult } from "./settingsShared";

export default function VaultSettingsPanel({
  settings,
  onUpdate,
  rebuilding,
  rebuildResult,
  onRebuildVectors,
}: {
  settings: SettingsState;
  onUpdate: SettingsUpdate;
  rebuilding: boolean;
  rebuildResult: TestResult | null;
  onRebuildVectors: () => void;
}) {
  const t = useT();
  return (
    <div className="space-y-6 max-w-lg">
      <div>
        <label className={labelClass}>{t("settings.ignoredFolders")}</label>
        <input type="text" className={inputClass} value={settings.ignoredFolders} onChange={e => onUpdate("ignoredFolders", e.target.value)} placeholder="node_modules, .git" />
        <p className={hintClass}>{t("settings.ignoredFoldersHint")}</p>
      </div>

      <div className="mt-10 rounded-lg border border-red-500/35 bg-red-500/5 p-5">
        <p className="text-sm font-medium text-red-400 mb-1">{t("settings.dangerZone")}</p>
        <p className="text-xs text-red-300/70 mb-4">{t("settings.dangerZoneHint")}</p>
        <button
          type="button"
          onClick={onRebuildVectors}
          disabled={rebuilding}
          className="border border-red-500/40 text-red-300 hover:bg-red-500/12 px-4 py-2 rounded-md text-sm cursor-pointer transition-colors disabled:opacity-50 disabled:cursor-not-allowed"
        >
          {rebuilding ? t("settings.rebuilding") : t("settings.rebuildIndex")}
        </button>
        {rebuildResult && (
          <p className={`text-xs mt-3 ${rebuildResult.ok ? "text-emerald-300" : "text-red-300"}`}>
            {rebuildResult.msg}
          </p>
        )}
      </div>
    </div>
  );
}
