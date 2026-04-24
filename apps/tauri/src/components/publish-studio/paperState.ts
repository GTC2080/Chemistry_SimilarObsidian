import type { PaperDocumentState } from "./types";

export function defaultPaperState(): PaperDocumentState {
  return {
    nodeIds: [],
    template: "standard-thesis",
    cslPath: "",
    bibliographyPath: "",
  };
}

export function parsePaperState(raw: string): PaperDocumentState {
  if (!raw.trim()) {
    return defaultPaperState();
  }

  try {
    const parsed = JSON.parse(raw) as Partial<PaperDocumentState> | null;
    if (!parsed || typeof parsed !== "object") {
      return defaultPaperState();
    }

    const seen = new Set<string>();
    const nodeIds = Array.isArray(parsed.nodeIds)
      ? parsed.nodeIds.filter((id): id is string => {
        if (typeof id !== "string") return false;
        const normalized = id.trim();
        if (!normalized) return false;
        if (seen.has(normalized)) return false;
        seen.add(normalized);
        return true;
      })
      : [];

    return {
      nodeIds,
      template: typeof parsed.template === "string" && parsed.template.trim()
        ? parsed.template
        : "standard-thesis",
      cslPath: typeof parsed.cslPath === "string" ? parsed.cslPath : "",
      bibliographyPath: typeof parsed.bibliographyPath === "string" ? parsed.bibliographyPath : "",
    };
  } catch {
    return defaultPaperState();
  }
}

export function serializePaperState(state: PaperDocumentState): string {
  return JSON.stringify(state, null, 2);
}
