export type DatabaseColumnType = "text" | "number" | "select" | "tags";

export interface DatabaseColumn {
  id: string;
  name: string;
  type: DatabaseColumnType;
}

export interface DatabaseRow {
  id: string;
  cells: Record<string, unknown>;
}

export interface DatabasePayload {
  columns: DatabaseColumn[];
  rows: DatabaseRow[];
}

const ALLOWED_TYPES: ReadonlySet<DatabaseColumnType> = new Set([
  "text",
  "number",
  "select",
  "tags",
]);

function createId(prefix: string): string {
  return `${prefix}_${Math.random().toString(36).slice(2, 10)}`;
}

function normalizeColumn(input: unknown): DatabaseColumn | null {
  if (!input || typeof input !== "object") return null;
  const value = input as Partial<DatabaseColumn>;
  const name = typeof value.name === "string" && value.name.trim()
    ? value.name.trim()
    : "Untitled";
  const id = typeof value.id === "string" && value.id.trim()
    ? value.id.trim()
    : createId("col");
  const type = value.type && ALLOWED_TYPES.has(value.type)
    ? value.type
    : "text";
  return { id, name, type };
}

function normalizeRow(input: unknown, columns: DatabaseColumn[]): DatabaseRow | null {
  if (!input || typeof input !== "object") return null;
  const value = input as Partial<DatabaseRow>;
  const id = typeof value.id === "string" && value.id.trim()
    ? value.id.trim()
    : createId("row");
  const rawCells = value.cells && typeof value.cells === "object" ? value.cells : {};
  const cells: Record<string, unknown> = {};
  for (const column of columns) {
    cells[column.id] = (rawCells as Record<string, unknown>)[column.id] ?? "";
  }
  return { id, cells };
}

export function createDefaultDatabasePayload(): DatabasePayload {
  const columns: DatabaseColumn[] = [
    { id: createId("col"), name: "Name", type: "text" },
    { id: createId("col"), name: "Tags", type: "tags" },
    { id: createId("col"), name: "Notes", type: "text" },
  ];
  const rows: DatabaseRow[] = Array.from({ length: 3 }, () => ({
    id: createId("row"),
    cells: Object.fromEntries(columns.map(column => [column.id, ""])),
  }));
  return { columns, rows };
}

export function normalizeDatabasePayload(input: unknown): DatabasePayload {
  const parsed = input && typeof input === "object" ? (input as Partial<DatabasePayload>) : {};
  const columns = Array.isArray(parsed.columns)
    ? parsed.columns.map(normalizeColumn).filter((v): v is DatabaseColumn => Boolean(v))
    : [];
  const safeColumns = columns.length > 0 ? columns : createDefaultDatabasePayload().columns;
  const rows = Array.isArray(parsed.rows)
    ? parsed.rows.map(row => normalizeRow(row, safeColumns)).filter((v): v is DatabaseRow => Boolean(v))
    : [];
  return { columns: safeColumns, rows };
}

export function parseDatabaseCodeBlock(markdown: string): DatabasePayload | null {
  const match = markdown.match(/^```database\s*[\r\n]+([\s\S]*?)\s*```$/);
  if (!match) return null;
  try {
    return normalizeDatabasePayload(JSON.parse(match[1]));
  } catch {
    return null;
  }
}

export function serializeDatabaseCodeBlock(payload: DatabasePayload): string {
  const normalized = normalizeDatabasePayload(payload);
  return `\`\`\`database\n${JSON.stringify(normalized, null, 2)}\n\`\`\``;
}
