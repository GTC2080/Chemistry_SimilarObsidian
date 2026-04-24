import { describe, expect, it } from "vitest";
import {
  DEFAULT_RUNTIME_SETTINGS,
  applyRuntimeSettings,
  normalizeDisciplineProfile,
  normalizeTheme,
} from "./settingsTypes";

describe("settingsTypes helpers", () => {
  it("normalizes theme values safely", () => {
    expect(normalizeTheme("light")).toBe("light");
    expect(normalizeTheme("dark")).toBe("dark");
    expect(normalizeTheme("sepia")).toBe(DEFAULT_RUNTIME_SETTINGS.theme);
  });

  it("normalizes discipline values safely", () => {
    expect(normalizeDisciplineProfile("chemistry")).toBe("chemistry");
    expect(normalizeDisciplineProfile("biology")).toBe(DEFAULT_RUNTIME_SETTINGS.activeDiscipline);
  });

  it("applies runtime settings to the document root", () => {
    applyRuntimeSettings({ uiLanguage: "en-US", theme: "light" });

    expect(document.documentElement.lang).toBe("en-US");
    expect(document.documentElement.getAttribute("data-theme")).toBe("light");
  });
});
