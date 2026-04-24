import { describe, expect, it } from "vitest";
import { getAdaptiveDebounceMs } from "./useSemanticResonance";

describe("semantic resonance helpers", () => {
  it("uses shorter debounce for smaller notes and longer debounce for bigger notes", () => {
    expect(getAdaptiveDebounceMs(120)).toBe(900);
    expect(getAdaptiveDebounceMs(900)).toBe(1500);
    expect(getAdaptiveDebounceMs(2600)).toBe(2200);
    expect(getAdaptiveDebounceMs(5000)).toBe(2800);
  });
});
