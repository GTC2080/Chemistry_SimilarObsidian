import { describe, expect, it } from "vitest";
import { getFileCategory } from "./types";

describe("getFileCategory", () => {
  it("maps known note formats", () => {
    expect(getFileCategory("md")).toBe("markdown");
    expect(getFileCategory("mol")).toBe("chem");
    expect(getFileCategory("paper")).toBe("paper");
    expect(getFileCategory("pdf")).toBe("pdf");
  });

  it("maps media and science formats case-insensitively", () => {
    expect(getFileCategory("PNG")).toBe("image");
    expect(getFileCategory("JDX")).toBe("spectroscopy");
    expect(getFileCategory("PDB")).toBe("molecular");
  });

  it("falls back to code for unknown extensions", () => {
    expect(getFileCategory("ts")).toBe("code");
  });
});
