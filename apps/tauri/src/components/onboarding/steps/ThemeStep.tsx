import { useT } from "../../../i18n";

interface ThemeStepProps {
  value: string;
  onChange: (value: string) => void;
}

const THEMES = [
  { value: "dark", labelKey: "onboarding.darkMode", icon: "🌙" },
  { value: "light", labelKey: "onboarding.lightMode", icon: "☀️" },
];

export default function ThemeStep({ value, onChange }: ThemeStepProps) {
  const t = useT();

  const handleChange = (theme: string) => {
    onChange(theme);
    document.documentElement.setAttribute("data-theme", theme);
  };

  return (
    <div className="flex flex-col items-center">
      <h2 className="text-lg font-semibold text-[var(--text-primary)] mb-2">{t("onboarding.chooseTheme")}</h2>
      <p className="text-sm text-[var(--text-tertiary)] mb-8">{t("onboarding.themeHint")}</p>
      <div className="flex gap-4">
        {THEMES.map(theme => (
          <button
            key={theme.value}
            type="button"
            onClick={() => handleChange(theme.value)}
            className="w-48 h-32 rounded-2xl border-2 flex flex-col items-center justify-center gap-2 transition-all duration-200 cursor-pointer"
            style={{
              borderColor: value === theme.value ? "var(--accent)" : "var(--separator-light)",
              background: value === theme.value ? "var(--accent-soft)" : "var(--surface-1)",
            }}
          >
            <span className="text-2xl">{theme.icon}</span>
            <span className="text-base font-semibold text-[var(--text-primary)]">{t(theme.labelKey)}</span>
          </button>
        ))}
      </div>
    </div>
  );
}
