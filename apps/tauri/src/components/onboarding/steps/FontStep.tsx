import { useT } from "../../../i18n";

interface FontStepProps {
  value: string;
  onChange: (value: string) => void;
}

const FONT_PRESETS = [
  { labelKey: "onboarding.systemDefault", value: "System Default" },
  { labelKey: "", value: "JetBrains Mono, monospace", fixedLabel: "JetBrains Mono" },
  { labelKey: "", value: "Fira Code, monospace", fixedLabel: "Fira Code" },
  { labelKey: "", value: "Cascadia Code, monospace", fixedLabel: "Cascadia Code" },
];

const PREVIEW_TEXT = 'const greeting = "Hello, Nexus!";\nfunction fibonacci(n) {\n  return n <= 1 ? n : fibonacci(n - 1) + fibonacci(n - 2);\n}';

export default function FontStep({ value, onChange }: FontStepProps) {
  const t = useT();
  const resolvedFont = value === "System Default"
    ? '-apple-system, BlinkMacSystemFont, "SF Pro Display", system-ui, sans-serif'
    : value;

  return (
    <div className="flex flex-col items-center w-full max-w-lg">
      <h2 className="text-lg font-semibold text-[var(--text-primary)] mb-2">{t("onboarding.editorFont")}</h2>
      <p className="text-sm text-[var(--text-tertiary)] mb-6">{t("onboarding.fontHint")}</p>

      <div className="flex flex-wrap gap-2 mb-4 justify-center">
        {FONT_PRESETS.map(preset => (
          <button
            key={preset.value}
            type="button"
            onClick={() => onChange(preset.value)}
            className="px-3 py-1.5 rounded-lg text-sm transition-all duration-200 cursor-pointer"
            style={{
              background: value === preset.value ? "var(--accent-soft)" : "var(--surface-1)",
              borderColor: value === preset.value ? "var(--accent)" : "var(--separator-light)",
              border: value === preset.value ? "1.5px solid var(--accent)" : "1px solid var(--separator-light)",
              color: value === preset.value ? "var(--text-primary)" : "var(--text-secondary)",
            }}
          >
            {preset.labelKey ? t(preset.labelKey) : preset.fixedLabel}
          </button>
        ))}
      </div>

      <input
        type="text"
        value={value}
        onChange={e => onChange(e.target.value)}
        placeholder={t("onboarding.fontPlaceholder")}
        className="w-full rounded-xl px-4 py-2.5 text-sm mb-4 transition-all bg-[var(--surface-1)] border border-[var(--separator-light)] text-[var(--text-secondary)] placeholder:text-[var(--text-quaternary)] focus:outline-none focus:border-[var(--accent)]"
      />

      <div
        className="w-full rounded-xl p-4 text-sm leading-relaxed whitespace-pre border border-[var(--separator-light)] bg-[var(--surface-1)] text-[var(--text-secondary)]"
        style={{ fontFamily: resolvedFont }}
      >
        {PREVIEW_TEXT}
      </div>
    </div>
  );
}
