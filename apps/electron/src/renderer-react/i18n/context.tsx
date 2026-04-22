import { createContext, useContext, useMemo, type ReactNode } from "react";
import type { Language, TranslationDict } from "./types";
import { zhCN } from "./zh-CN";
import { en } from "./en";

const dicts: Record<Language, TranslationDict> = { "zh-CN": zhCN, en };

type TFunction = (key: string, params?: Record<string, string | number>) => string;

interface LanguageContextValue {
  t: TFunction;
  language: Language;
}

const LanguageContext = createContext<LanguageContextValue>({
  t: (key) => key,
  language: "zh-CN",
});

export function LanguageProvider({ language, children }: { language: string; children: ReactNode }) {
  const lang: Language = language === "en" ? "en" : "zh-CN";
  const value = useMemo<LanguageContextValue>(() => {
    const dict = dicts[lang];
    const t: TFunction = (key, params) => {
      let text = dict[key] ?? key;
      if (params) {
        for (const [k, v] of Object.entries(params)) {
          text = text.split(`{${k}}`).join(String(v));
        }
      }
      return text;
    };
    return { t, language: lang };
  }, [lang]);

  return <LanguageContext.Provider value={value}>{children}</LanguageContext.Provider>;
}

export function useT(): TFunction {
  return useContext(LanguageContext).t;
}

export function useLanguage(): Language {
  return useContext(LanguageContext).language;
}

export { LanguageContext };
