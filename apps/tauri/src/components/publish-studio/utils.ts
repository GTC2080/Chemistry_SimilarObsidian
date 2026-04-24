export function toTransformString(
  transform: { x: number; y: number; scaleX: number; scaleY: number } | null
): string | undefined {
  if (!transform) return undefined;
  return `translate3d(${transform.x}px, ${transform.y}px, 0) scaleX(${transform.scaleX}) scaleY(${transform.scaleY})`;
}

function normalizePathLike(path: string): string {
  return path.replace(/\\/g, "/");
}

function resolveResourcePath(notePath: string, imageRef: string): string | null {
  const cleaned = normalizePathLike(imageRef.trim()).replace(/^<|>$/g, "");
  if (!cleaned) return null;

  const lowered = cleaned.toLowerCase();
  if (
    lowered.startsWith("http://")
    || lowered.startsWith("https://")
    || lowered.startsWith("data:")
    || lowered.startsWith("file:")
  ) {
    return null;
  }

  if (/^[a-zA-Z]:\//.test(cleaned) || cleaned.startsWith("/")) {
    return cleaned;
  }

  const normalizedNote = normalizePathLike(notePath);
  const noteDir = normalizedNote.includes("/")
    ? normalizedNote.slice(0, normalizedNote.lastIndexOf("/"))
    : "";
  return noteDir ? `${noteDir}/${cleaned}` : cleaned;
}

export function collectImagePaths(markdown: string, notePath: string): string[] {
  const matches = markdown.matchAll(/!\[[^\]]*]\(([^)\s]+)(?:\s+["'][^"']*["'])?\)/g);
  const paths: string[] = [];
  for (const match of matches) {
    const value = match[1];
    if (!value) continue;
    const resolved = resolveResourcePath(notePath, value);
    if (resolved) paths.push(resolved);
  }
  return paths;
}

function stoichiometryToTable(blockContent: string): string {
  const rows = blockContent
    .split(/\r?\n/)
    .map(line => line.trim())
    .filter(line => line.length > 0)
    .map(line => line.split(/[\t, ]+/).filter(cell => cell.length > 0));

  if (rows.length === 0) {
    return "| Value |\n| --- |\n|  |\n";
  }

  const maxColumns = Math.max(...rows.map(row => row.length), 1);
  const header = `| ${Array.from({ length: maxColumns }, (_, idx) => `C${idx + 1}`).join(" | ")} |`;
  const separator = `| ${Array.from({ length: maxColumns }, () => "---").join(" | ")} |`;
  const body = rows
    .map(row => `| ${Array.from({ length: maxColumns }, (_, idx) => row[idx] ?? "").join(" | ")} |`)
    .join("\n");

  return `${header}\n${separator}\n${body}\n`;
}

export function preprocessMarkdown(markdown: string): string {
  return markdown.replace(/```stoichiometry\s*\n?([\s\S]*?)```/gi, (_, content: string) => {
    return stoichiometryToTable(content);
  });
}
