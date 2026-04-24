export interface StoichiometryRow {
  id: string;
  name: string;
  formula: string;
  mw: number;
  eq: number;
  moles: number; // mmol
  mass: number; // mg
  volume: number; // uL
  isReference: boolean;
  density?: number;
}

function createId(prefix: string): string {
  return `${prefix}_${Math.random().toString(36).slice(2, 10)}`;
}

function toNumber(value: unknown, fallback = 0): number {
  const num =
    typeof value === "number"
      ? value
      : typeof value === "string"
        ? Number(value)
        : NaN;
  if (!Number.isFinite(num)) return fallback;
  return num;
}

function toNonNegativeNumber(value: unknown, fallback = 0): number {
  return Math.max(0, toNumber(value, fallback));
}

function normalizeRow(input: unknown, index: number): StoichiometryRow | null {
  if (!input || typeof input !== "object") return null;
  const row = input as Partial<StoichiometryRow>;
  const id = typeof row.id === "string" && row.id.trim() ? row.id.trim() : createId("sto");
  const name = typeof row.name === "string" ? row.name : "";
  const formula = typeof row.formula === "string" ? row.formula : "";
  const mw = toNonNegativeNumber(row.mw);
  const eq = toNonNegativeNumber(row.eq, index === 0 ? 1 : 0);
  const moles = toNonNegativeNumber(row.moles);
  const mass = toNonNegativeNumber(row.mass);
  const volume = toNonNegativeNumber(row.volume);
  const densityRaw = toNumber(row.density, NaN);
  const density = Number.isFinite(densityRaw) && densityRaw > 0 ? densityRaw : undefined;
  return {
    id,
    name,
    formula,
    mw,
    eq,
    moles,
    mass,
    volume,
    isReference: Boolean(row.isReference),
    density,
  };
}

function createFallbackRow(index: number): StoichiometryRow {
  return {
    id: createId("sto"),
    name: "",
    formula: "",
    mw: 0,
    eq: index === 0 ? 1 : 0,
    moles: 0,
    mass: 0,
    volume: 0,
    isReference: index === 0,
    density: undefined,
  };
}

export function createDefaultStoichiometryRows(): StoichiometryRow[] {
  return recalculateStoichiometryRows([
    {
      id: createId("sto"),
      name: "",
      formula: "",
      mw: 0,
      eq: 1,
      moles: 1,
      mass: 0,
      volume: 0,
      isReference: true,
    },
    {
      id: createId("sto"),
      name: "",
      formula: "",
      mw: 0,
      eq: 1,
      moles: 0,
      mass: 0,
      volume: 0,
      isReference: false,
    },
  ]);
}

export function normalizeStoichiometryRows(input: unknown): StoichiometryRow[] {
  const parsed = Array.isArray(input)
    ? input
        .map((row, index) => normalizeRow(row, index))
        .filter((row): row is StoichiometryRow => Boolean(row))
    : [];
  const safeRows = parsed.length > 0 ? parsed : createDefaultStoichiometryRows();
  return recalculateStoichiometryRows(safeRows);
}

export function recalculateStoichiometryRows(input: StoichiometryRow[]): StoichiometryRow[] {
  const rows = input.length > 0 ? input : createDefaultStoichiometryRows();
  const normalizedRows = rows.map((row, index) => normalizeRow(row, index) ?? createFallbackRow(index));

  const referenceIndex = Math.max(
    0,
    normalizedRows.findIndex(row => row.isReference)
  );
  const referenceMoles = toNonNegativeNumber(normalizedRows[referenceIndex]?.moles, 0);

  return normalizedRows.map((row, index) => {
    const isReference = index === referenceIndex;
    const eq = isReference ? 1 : toNonNegativeNumber(row.eq);
    const moles = isReference ? referenceMoles : referenceMoles * eq;
    const mw = toNonNegativeNumber(row.mw);
    const mass = moles * mw;
    const inferredDensity =
      row.density && row.density > 0
        ? row.density
        : row.mass > 0 && row.volume > 0
          ? row.mass / row.volume
          : undefined;
    const volume = inferredDensity ? mass / inferredDensity : 0;

    return {
      ...row,
      isReference,
      eq,
      moles,
      mw,
      mass,
      volume,
      density: inferredDensity,
    };
  });
}

export function parseStoichiometryCodeBlock(markdown: string): StoichiometryRow[] | null {
  const match = markdown.match(/^```stoichiometry\s*[\r\n]+([\s\S]*?)\s*```$/);
  if (!match) return null;
  try {
    return normalizeStoichiometryRows(JSON.parse(match[1]));
  } catch {
    return null;
  }
}

export function serializeStoichiometryCodeBlock(rows: StoichiometryRow[]): string {
  const normalized = normalizeStoichiometryRows(rows);
  return `\`\`\`stoichiometry\n${JSON.stringify(normalized, null, 2)}\n\`\`\``;
}
