import { useState, useCallback, useEffect } from "react";
import { useT } from "../../i18n";
import { persistStoreValues } from "../../utils/settingsStore";
import { applyRuntimeSettings, DEFAULT_RUNTIME_SETTINGS, type RuntimeSettings } from "../settings/settingsTypes";
import type { DisciplineProfile } from "../settings/settingsTypes";
import LanguageStep from "./steps/LanguageStep";
import ThemeStep from "./steps/ThemeStep";
import FontStep from "./steps/FontStep";
import DisciplineStep from "./steps/DisciplineStep";

const STEP_COUNT = 4;
const TRANSITION_DURATION = 400;

interface OnboardingWizardProps {
  onComplete: (settings: RuntimeSettings) => void;
  onLanguageChange?: (lang: string) => void;
}

interface OnboardingPreferences {
  uiLanguage: string;
  theme: "dark" | "light";
  fontFamily: string;
  activeDiscipline: DisciplineProfile;
}

export default function OnboardingWizard({ onComplete, onLanguageChange }: OnboardingWizardProps) {
  const t = useT();
  const [step, setStep] = useState(0);
  const [direction, setDirection] = useState<"forward" | "backward">("forward");
  const [animating, setAnimating] = useState(false);
  const [prefs, setPrefs] = useState<OnboardingPreferences>({
    uiLanguage: DEFAULT_RUNTIME_SETTINGS.uiLanguage,
    theme: DEFAULT_RUNTIME_SETTINGS.theme,
    fontFamily: DEFAULT_RUNTIME_SETTINGS.fontFamily,
    activeDiscipline: DEFAULT_RUNTIME_SETTINGS.activeDiscipline,
  });

  useEffect(() => {
    const originalTheme = document.documentElement.getAttribute("data-theme") || "dark";
    return () => {
      document.documentElement.setAttribute("data-theme", originalTheme);
    };
  }, []);

  const updatePref = useCallback(<K extends keyof OnboardingPreferences>(key: K, value: OnboardingPreferences[K]) => {
    setPrefs(prev => ({ ...prev, [key]: value }));
  }, []);

  const goTo = useCallback((next: number) => {
    if (animating) return;
    setDirection(next > step ? "forward" : "backward");
    setAnimating(true);
    setStep(next);
    setTimeout(() => setAnimating(false), TRANSITION_DURATION);
  }, [step, animating]);

  const handleFinish = useCallback(async () => {
    const runtime: RuntimeSettings = {
      ...DEFAULT_RUNTIME_SETTINGS,
      uiLanguage: prefs.uiLanguage,
      theme: prefs.theme,
      fontFamily: prefs.fontFamily,
      activeDiscipline: prefs.activeDiscipline,
    };

    await persistStoreValues({
      uiLanguage: prefs.uiLanguage,
      theme: prefs.theme,
      fontFamily: prefs.fontFamily,
      activeDiscipline: prefs.activeDiscipline,
      onboardingCompleted: true,
    });

    applyRuntimeSettings(runtime);
    onComplete(runtime);
  }, [prefs, onComplete]);

  const welcomeText = step === 0
    ? t("onboarding.welcomeFull")
    : t("onboarding.welcome");

  const prevLabel = t("onboarding.prev");
  const nextLabel = t("onboarding.next");
  const finishLabel = t("onboarding.start");

  const steps = [
    <LanguageStep key="lang" value={prefs.uiLanguage} onChange={v => { updatePref("uiLanguage", v); onLanguageChange?.(v); }} />,
    <ThemeStep key="theme" value={prefs.theme} onChange={v => updatePref("theme", v as "dark" | "light")} />,
    <FontStep key="font" value={prefs.fontFamily} onChange={v => updatePref("fontFamily", v)} />,
    <DisciplineStep key="disc" value={prefs.activeDiscipline} onChange={v => updatePref("activeDiscipline", v)} />,
  ];

  return (
    <div className="flex-1 min-h-0 flex flex-col items-center justify-center" style={{ background: "var(--surface-0)" }}>
      <div className="mb-10 text-center">
        <h1 className="text-2xl font-bold text-[var(--text-primary)] tracking-tight">{welcomeText}</h1>
      </div>

      <div className="w-full max-w-2xl px-8 flex items-center justify-center min-h-[280px]">
        <div
          key={step}
          style={{
            animation: `${direction === "forward" ? "slideForward" : "slideBackward"} ${TRANSITION_DURATION}ms cubic-bezier(0.22, 1, 0.36, 1) both`,
          }}
        >
          {steps[step]}
        </div>
      </div>

      <div className="flex gap-2 mt-10 mb-6">
        {Array.from({ length: STEP_COUNT }, (_, i) => (
          <div
            key={i}
            className="w-2 h-2 rounded-full transition-all duration-300"
            style={{
              background: i === step ? "var(--accent)" : "var(--separator)",
              transform: i === step ? "scale(1.3)" : "scale(1)",
            }}
          />
        ))}
      </div>

      <div className="flex gap-3">
        {step > 0 && (
          <button
            type="button"
            onClick={() => goTo(step - 1)}
            disabled={animating}
            className="px-6 py-2.5 rounded-xl text-sm cursor-pointer transition-colors text-[var(--text-tertiary)] hover:text-[var(--text-primary)] hover:bg-[var(--sidebar-hover)]"
          >
            {prevLabel}
          </button>
        )}
        {step < STEP_COUNT - 1 ? (
          <button
            type="button"
            onClick={() => goTo(step + 1)}
            disabled={animating}
            className="px-6 py-2.5 rounded-xl text-sm font-medium cursor-pointer transition-all hover:brightness-110"
            style={{
              background: "linear-gradient(135deg, #0A84FF 0%, #0066D6 100%)",
              color: "#fff",
              boxShadow: "0 6px 18px rgba(10,132,255,0.28), inset 0 1px 0 rgba(255,255,255,0.18)",
            }}
          >
            {nextLabel}
          </button>
        ) : (
          <button
            type="button"
            onClick={() => { void handleFinish(); }}
            disabled={animating}
            className="px-6 py-2.5 rounded-xl text-sm font-medium cursor-pointer transition-all hover:brightness-110"
            style={{
              background: "linear-gradient(135deg, #0A84FF 0%, #0066D6 100%)",
              color: "#fff",
              boxShadow: "0 6px 18px rgba(10,132,255,0.28), inset 0 1px 0 rgba(255,255,255,0.18)",
            }}
          >
            {finishLabel}
          </button>
        )}
      </div>
    </div>
  );
}
