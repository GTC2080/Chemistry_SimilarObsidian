import { useT } from "../../../i18n";
import type { DisciplineProfile } from "../../settings/settingsTypes";

interface DisciplineStepProps {
  value: DisciplineProfile;
  onChange: (value: DisciplineProfile) => void;
}

interface DisciplineCard {
  id: DisciplineProfile | string;
  labelKey: string;
  icon: string;
  available: boolean;
}

const DISCIPLINES: DisciplineCard[] = [
  { id: "chemistry", labelKey: "onboarding.chemistry", icon: "⚗️", available: true },
  { id: "physics", labelKey: "onboarding.physics", icon: "⚛️", available: false },
  { id: "biology", labelKey: "onboarding.biology", icon: "🧬", available: false },
  { id: "math", labelKey: "onboarding.math", icon: "📐", available: false },
];

export default function DisciplineStep({ value, onChange }: DisciplineStepProps) {
  const t = useT();
  return (
    <div className="flex flex-col items-center">
      <h2 className="text-lg font-semibold text-[var(--text-primary)] mb-2">{t("onboarding.discipline")}</h2>
      <p className="text-sm text-[var(--text-tertiary)] mb-8">{t("onboarding.disciplineHint")}</p>
      <div className="grid grid-cols-2 gap-4">
        {DISCIPLINES.map(d => (
          <button
            key={d.id}
            type="button"
            disabled={!d.available}
            onClick={() => { if (d.available) onChange(d.id as DisciplineProfile); }}
            className="w-44 h-32 rounded-2xl border-2 flex flex-col items-center justify-center gap-2 transition-all duration-200 relative"
            style={{
              cursor: d.available ? "pointer" : "not-allowed",
              opacity: d.available ? 1 : 0.45,
              borderColor: d.available && value === d.id ? "var(--accent)" : "var(--separator-light)",
              background: d.available && value === d.id ? "var(--accent-soft)" : "var(--surface-1)",
            }}
          >
            <span className="text-2xl">{d.icon}</span>
            <span className="text-base font-semibold text-[var(--text-primary)]">{t(d.labelKey)}</span>
            {!d.available && (
              <span className="absolute top-2 right-2 text-[10px] px-1.5 py-0.5 rounded-full bg-[var(--surface-2)] text-[var(--text-quaternary)]">
                {t("onboarding.comingSoon")}
              </span>
            )}
          </button>
        ))}
      </div>
    </div>
  );
}
