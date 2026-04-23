import { listEntries, listRecentNotes, readNote, writeNote, type HostEntry, type HostNote } from "./host-shell";

export interface NoteRecord {
  id: string;
  name: string;
  relPath: string;
  fileExtension: string;
  updatedAtMs: number;
  title: string;
}

export interface TreeNode {
  name: string;
  relPath: string;
  isFolder: boolean;
  children: TreeNode[];
  note: NoteRecord | null;
}

function ext(name: string) {
  const dot = name.lastIndexOf(".");
  return dot >= 0 ? name.slice(dot + 1).toLowerCase() : "";
}

export function createNoteRecord(entry: HostEntry | HostNote): NoteRecord {
  const name = entry.name || entry.relPath.split("/").filter(Boolean).pop() || "Untitled.md";
  return {
    id: entry.relPath,
    name,
    relPath: entry.relPath,
    fileExtension: ext(name),
    updatedAtMs: entry.mtimeMs,
    title: entry.title || name.replace(/\.[^.]+$/, "")
  };
}

function insertNode(roots: TreeNode[], entry: HostEntry) {
  const parts = entry.relPath.split("/").filter(Boolean);
  let cursor = roots;
  let rel = "";

  for (let index = 0; index < parts.length; index += 1) {
    const name = parts[index];
    rel = rel ? `${rel}/${name}` : name;
    const isLeaf = index === parts.length - 1;
    let existing = cursor.find((item) => item.relPath === rel);

    if (!existing) {
      existing = {
        name,
        relPath: rel,
        isFolder: isLeaf ? entry.isDirectory : true,
        children: [],
        note: isLeaf && !entry.isDirectory && entry.kind === "note" ? createNoteRecord(entry) : null
      };
      cursor.push(existing);
    }

    cursor = existing.children;
  }
}

function sortTree(nodes: TreeNode[]) {
  nodes.sort((left, right) => {
    if (left.isFolder !== right.isFolder) {
      return left.isFolder ? -1 : 1;
    }
    return left.name.localeCompare(right.name, undefined, { sensitivity: "base" });
  });
  for (const node of nodes) {
    sortTree(node.children);
  }
}

export async function scanVaultTree() {
  const queue = [""];
  const flat: HostEntry[] = [];

  while (queue.length > 0) {
    const parentRelPath = queue.shift() ?? "";
    const envelope = await listEntries(parentRelPath);
    if (!envelope?.ok || !envelope.data) {
      return {
        ok: false,
        error: envelope?.error ?? { code: "HOST_INTERNAL_ERROR", message: "Failed to scan vault tree.", details: null }
      };
    }

    for (const item of envelope.data.items ?? []) {
      flat.push(item);
      if (item.isDirectory) {
        queue.push(item.relPath);
      }
    }
  }

  const tree: TreeNode[] = [];
  for (const entry of flat) {
    insertNode(tree, entry);
  }
  sortTree(tree);

  return {
    ok: true,
    data: {
      tree,
      notes: flat.filter((item) => item.kind === "note" && !item.isDirectory).map(createNoteRecord)
    }
  };
}

export async function loadRecentNotes() {
  const envelope = await listRecentNotes();
  if (!envelope?.ok || !envelope.data) {
    return [];
  }

  return (envelope.data.items ?? []).filter((item) => item.kind === "note" && !item.isDirectory).map(createNoteRecord);
}

export async function loadNoteContent(relPath: string) {
  return readNote(relPath);
}

export async function saveNoteContent(relPath: string, bodyText: string, expectedRevision: string | null) {
  return writeNote(relPath, bodyText, expectedRevision);
}
