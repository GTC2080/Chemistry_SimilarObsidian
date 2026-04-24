import { useT } from "../../../i18n";

interface LanguageStepProps {
  value: string;
  onChange: (value: string) => void;
}

const LANGUAGES = [
  { value: "zh-CN", labelKey: "onboarding.zhCN", descKey: "onboarding.en" },
  { value: "en", labelKey: "onboarding.en", descKey: "onboarding.zhCN" },
];

export default function LanguageStep({ value, onChange }: LanguageStepProps) {
  const t = useT();
  return (
    <div className="flex flex-col items-center">
      <h2 className="text-lg font-semibold text-[var(--text-primary)] mb-2">{t("onboarding.chooseLanguage")}</h2>
      <p className="text-sm text-[var(--text-tertiary)] mb-8">{t("onboarding.chooseLanguageHint")}</p>
      <div className="flex gap-4">
        {LANGUAGES.map(lang => (
          <button
            key={lang.value}
            type="button"
            onClick={() => onChange(lang.value)}
            className="w-48 h-32 rounded-2xl border-2 flex flex-col items-center justify-center gap-2 transition-all duration-200 cursor-pointer"
            style={{
              borderColor: value === lang.value ? "var(--accent)" : "var(--separator-light)",
              background: value === lang.value ? "var(--accent-soft)" : "var(--surface-1)",
            }}
          >
            <span className="text-xl font-semibold text-[var(--text-primary)]">{t(lang.labelKey)}</span>
            <span className="text-xs text-[var(--text-tertiary)]">{t(lang.descKey)}</span>
          </button>
        ))}
      </div>
    </div>
  );
}
