const fs = require("node:fs/promises");
const path = require("node:path");
const { HOST_ERROR_CODES } = require("../shared/host-contract");

const FILES_LIST_LIMIT_MAX = 256;

class FilesSurfaceError extends Error {
  constructor(code, message, details = null) {
    super(message);
    this.code = code;
    this.details = details;
  }
}

function normalizeVaultRoot(vaultPath) {
  return path.resolve(vaultPath);
}

function normalizeRelPath(relPath) {
  if (!relPath) {
    return "";
  }

  return String(relPath)
    .replaceAll("\\", "/")
    .replace(/^\/+/, "")
    .replace(/\/{2,}/g, "/")
    .trim();
}

function resolveVaultPath(vaultPath, relPath = "") {
  const root = normalizeVaultRoot(vaultPath);
  const target = path.resolve(root, normalizeRelPath(relPath));
  const relative = path.relative(root, target);

  if (relative.startsWith("..") || path.isAbsolute(relative)) {
    throw new FilesSurfaceError(
      HOST_ERROR_CODES.invalidArgument,
      "Requested relPath must stay within the active vault.",
      {
        field: "relPath"
      }
    );
  }

  return {
    root,
    target
  };
}

function toPublicRelPath(vaultRoot, absolutePath) {
  return path.relative(vaultRoot, absolutePath).split(path.sep).join("/");
}

function isHiddenName(name) {
  return typeof name === "string" && name.startsWith(".");
}

function deriveEntryKind(dirent, absolutePath) {
  if (dirent.isDirectory()) {
    return "directory";
  }

  const extension = path.extname(absolutePath).toLowerCase();
  if (extension === ".md") {
    return "note";
  }

  return "attachment";
}

function deriveEntryKindFromStats(stats, absolutePath) {
  if (stats.isDirectory()) {
    return "directory";
  }

  const extension = path.extname(absolutePath).toLowerCase();
  if (extension === ".md") {
    return "note";
  }

  return "attachment";
}

function deriveDisplayTitle(entryName) {
  const extension = path.extname(entryName);
  return extension ? entryName.slice(0, -extension.length) : entryName;
}

function compareEntries(left, right) {
  if (left.is_directory !== right.is_directory) {
    return left.is_directory ? -1 : 1;
  }

  return left.rel_path.localeCompare(right.rel_path, undefined, { sensitivity: "base" });
}

async function statSafe(absolutePath) {
  try {
    return await fs.stat(absolutePath);
  } catch (error) {
    if (error && error.code === "ENOENT") {
      throw new FilesSurfaceError(
        HOST_ERROR_CODES.kernelNotFound,
        "Requested content path was not found inside the active vault.",
        null
      );
    }

    throw new FilesSurfaceError(
      HOST_ERROR_CODES.kernelIoError,
      "Failed to inspect content path inside the active vault.",
      {
        path: absolutePath
      }
    );
  }
}

async function pathExists(absolutePath) {
  try {
    await fs.access(absolutePath);
    return true;
  } catch {
    return false;
  }
}

function ensureSimpleName(value, fieldName) {
  const name = typeof value === "string" ? value.trim() : "";
  if (
    !name ||
    name === "." ||
    name === ".." ||
    name.includes("/") ||
    name.includes("\\") ||
    isHiddenName(name)
  ) {
    throw new FilesSurfaceError(
      HOST_ERROR_CODES.invalidArgument,
      `${fieldName} must be a visible single path segment.`,
      {
        field: fieldName
      }
    );
  }

  return name;
}

async function buildEntry(root, absolutePath, stats = null) {
  const actualStats = stats ?? await statSafe(absolutePath);
  const name = path.basename(absolutePath);
  const kind = deriveEntryKindFromStats(actualStats, absolutePath);

  return {
    rel_path: toPublicRelPath(root, absolutePath),
    name,
    title: deriveDisplayTitle(name),
    kind,
    is_directory: actualStats.isDirectory(),
    size_bytes: actualStats.isDirectory() ? 0 : Number(actualStats.size ?? 0),
    mtime_ms: Number(actualStats.mtimeMs ?? 0)
  };
}

async function listEntries(opts = {}) {
  const {
    vaultPath,
    parentRelPath = "",
    limit = 64
  } = opts;

  const clampedLimit = Math.min(limit, FILES_LIST_LIMIT_MAX);
  const { root, target } = resolveVaultPath(vaultPath, parentRelPath);
  const parentStat = await statSafe(target);

  if (!parentStat.isDirectory()) {
    throw new FilesSurfaceError(
      HOST_ERROR_CODES.invalidArgument,
      "parentRelPath must resolve to a directory inside the active vault.",
      {
        field: "parentRelPath"
      }
    );
  }

  let dirents;
  try {
    dirents = await fs.readdir(target, { withFileTypes: true });
  } catch (error) {
    throw new FilesSurfaceError(
      HOST_ERROR_CODES.kernelIoError,
      "Failed to enumerate vault entries.",
      {
        parentRelPath: normalizeRelPath(parentRelPath)
      }
    );
  }

  const visibleDirents = dirents.filter((dirent) => !isHiddenName(dirent.name));
  const entries = await Promise.all(visibleDirents.map(async (dirent) => {
    const absolutePath = path.join(target, dirent.name);
    const stats = await statSafe(absolutePath);
    const relPath = toPublicRelPath(root, absolutePath);
    const kind = deriveEntryKind(dirent, absolutePath);

    return {
      rel_path: relPath,
      name: dirent.name,
      title: deriveDisplayTitle(dirent.name),
      kind,
      is_directory: dirent.isDirectory(),
      size_bytes: dirent.isDirectory() ? 0 : Number(stats.size ?? 0),
      mtime_ms: Number(stats.mtimeMs ?? 0)
    };
  }));

  entries.sort(compareEntries);

  return {
    parent_rel_path: normalizeRelPath(parentRelPath),
    entries: entries.slice(0, clampedLimit)
  };
}

async function createFolder(opts = {}) {
  const {
    vaultPath,
    parentRelPath = "",
    folderName
  } = opts;

  const name = ensureSimpleName(folderName, "folderName");
  const { root, target: parentTarget } = resolveVaultPath(vaultPath, parentRelPath);
  const parentStat = await statSafe(parentTarget);
  if (!parentStat.isDirectory()) {
    throw new FilesSurfaceError(
      HOST_ERROR_CODES.invalidArgument,
      "parentRelPath must resolve to a directory inside the active vault.",
      {
        field: "parentRelPath"
      }
    );
  }

  const target = path.join(parentTarget, name);
  if (await pathExists(target)) {
    throw new FilesSurfaceError(
      HOST_ERROR_CODES.kernelConflict,
      "A vault entry already exists at the requested folder path.",
      {
        relPath: toPublicRelPath(root, target)
      }
    );
  }

  try {
    await fs.mkdir(target, { recursive: false });
  } catch (error) {
    if (error && error.code === "EEXIST") {
      throw new FilesSurfaceError(
        HOST_ERROR_CODES.kernelConflict,
        "A vault entry already exists at the requested folder path.",
        {
          relPath: toPublicRelPath(root, target)
        }
      );
    }

    throw new FilesSurfaceError(
      HOST_ERROR_CODES.kernelIoError,
      "Failed to create folder inside the active vault.",
      {
        parentRelPath: normalizeRelPath(parentRelPath),
        folderName: name
      }
    );
  }

  return {
    disposition: "created",
    entry: await buildEntry(root, target)
  };
}

async function renameEntry(opts = {}) {
  const {
    vaultPath,
    fromRelPath,
    toRelPath
  } = opts;

  const normalizedFrom = normalizeRelPath(fromRelPath);
  const normalizedTo = normalizeRelPath(toRelPath);
  if (!normalizedFrom || !normalizedTo) {
    throw new FilesSurfaceError(
      HOST_ERROR_CODES.invalidArgument,
      "fromRelPath and toRelPath must be non-empty vault-relative paths.",
      {
        field: !normalizedFrom ? "fromRelPath" : "toRelPath"
      }
    );
  }

  const { root, target: sourceTarget } = resolveVaultPath(vaultPath, normalizedFrom);
  const { target: destinationTarget } = resolveVaultPath(vaultPath, normalizedTo);
  const sourceStat = await statSafe(sourceTarget);

  if (sourceTarget === destinationTarget) {
    return {
      disposition: "no_op",
      entry: await buildEntry(root, sourceTarget, sourceStat)
    };
  }

  if (await pathExists(destinationTarget)) {
    throw new FilesSurfaceError(
      HOST_ERROR_CODES.kernelConflict,
      "A vault entry already exists at the requested destination path.",
      {
        relPath: normalizedTo
      }
    );
  }

  const destinationParent = path.dirname(destinationTarget);
  const destinationParentStat = await statSafe(destinationParent);
  if (!destinationParentStat.isDirectory()) {
    throw new FilesSurfaceError(
      HOST_ERROR_CODES.invalidArgument,
      "The destination parent path must resolve to a directory inside the active vault.",
      {
        field: "toRelPath"
      }
    );
  }

  try {
    await fs.rename(sourceTarget, destinationTarget);
  } catch (error) {
    if (error && error.code === "ENOENT") {
      throw new FilesSurfaceError(
        HOST_ERROR_CODES.kernelNotFound,
        "The source vault entry was not found.",
        {
          relPath: normalizedFrom
        }
      );
    }

    throw new FilesSurfaceError(
      HOST_ERROR_CODES.kernelIoError,
      "Failed to rename or move vault entry.",
      {
        fromRelPath: normalizedFrom,
        toRelPath: normalizedTo
      }
    );
  }

  return {
    disposition: "renamed",
    entry: await buildEntry(root, destinationTarget)
  };
}

async function deleteEntry(opts = {}) {
  const {
    vaultPath,
    relPath
  } = opts;

  const normalizedRelPath = normalizeRelPath(relPath);
  if (!normalizedRelPath) {
    throw new FilesSurfaceError(
      HOST_ERROR_CODES.invalidArgument,
      "relPath must point to a vault entry, not the vault root.",
      {
        field: "relPath"
      }
    );
  }

  const { target } = resolveVaultPath(vaultPath, normalizedRelPath);
  const stats = await statSafe(target);
  const kind = deriveEntryKindFromStats(stats, target);

  try {
    await fs.rm(target, {
      recursive: stats.isDirectory(),
      force: false
    });
  } catch (error) {
    if (error && error.code === "ENOENT") {
      throw new FilesSurfaceError(
        HOST_ERROR_CODES.kernelNotFound,
        "The requested vault entry was not found.",
        {
          relPath: normalizedRelPath
        }
      );
    }

    throw new FilesSurfaceError(
      HOST_ERROR_CODES.kernelIoError,
      "Failed to delete vault entry.",
      {
        relPath: normalizedRelPath
      }
    );
  }

  return {
    disposition: "deleted",
    deleted: true,
    rel_path: normalizedRelPath,
    kind,
    is_directory: stats.isDirectory()
  };
}

async function readNote(opts = {}) {
  const {
    vaultPath,
    relPath
  } = opts;

  const normalizedRelPath = normalizeRelPath(relPath);
  const { root, target } = resolveVaultPath(vaultPath, normalizedRelPath);
  const stats = await statSafe(target);

  if (!stats.isFile()) {
    throw new FilesSurfaceError(
      HOST_ERROR_CODES.invalidArgument,
      "relPath must resolve to a note file inside the active vault.",
      {
        field: "relPath"
      }
    );
  }

  if (path.extname(target).toLowerCase() !== ".md") {
    throw new FilesSurfaceError(
      HOST_ERROR_CODES.invalidArgument,
      "Only Markdown note files can be read through files.readNote in the current baseline.",
      {
        field: "relPath"
      }
    );
  }

  let bodyText = "";
  try {
    bodyText = await fs.readFile(target, "utf8");
  } catch {
    throw new FilesSurfaceError(
      HOST_ERROR_CODES.kernelIoError,
      "Failed to read note content from the active vault.",
      {
        relPath: normalizedRelPath
      }
    );
  }

  const fileName = path.basename(target);
  return {
    rel_path: toPublicRelPath(root, target),
    name: fileName,
    title: deriveTitleFromBody(fileName, bodyText),
    kind: "note",
    body_text: bodyText,
    size_bytes: Number(stats.size ?? 0),
    mtime_ms: Number(stats.mtimeMs ?? 0)
  };
}

async function listRecentNotes(opts = {}) {
  const {
    vaultPath,
    limit = 16
  } = opts;

  const clampedLimit = Math.min(limit, FILES_LIST_LIMIT_MAX);
  const root = normalizeVaultRoot(vaultPath);
  const noteEntries = [];
  const pendingDirectories = [root];

  while (pendingDirectories.length > 0) {
    const currentDir = pendingDirectories.pop();
    let dirents;
    try {
      dirents = await fs.readdir(currentDir, { withFileTypes: true });
    } catch {
      throw new FilesSurfaceError(
        HOST_ERROR_CODES.kernelIoError,
        "Failed to enumerate recent content inside the active vault.",
        null
      );
    }

    for (const dirent of dirents) {
      if (isHiddenName(dirent.name)) {
        continue;
      }

      const absolutePath = path.join(currentDir, dirent.name);
      if (dirent.isDirectory()) {
        pendingDirectories.push(absolutePath);
        continue;
      }

      if (path.extname(dirent.name).toLowerCase() !== ".md") {
        continue;
      }

      const stats = await statSafe(absolutePath);
      noteEntries.push({
        rel_path: toPublicRelPath(root, absolutePath),
        name: dirent.name,
        title: deriveDisplayTitle(dirent.name),
        kind: "note",
        is_directory: false,
        size_bytes: Number(stats.size ?? 0),
        mtime_ms: Number(stats.mtimeMs ?? 0)
      });
    }
  }

  noteEntries.sort((left, right) => {
    if (right.mtime_ms !== left.mtime_ms) {
      return right.mtime_ms - left.mtime_ms;
    }

    return left.rel_path.localeCompare(right.rel_path, undefined, { sensitivity: "base" });
  });

  return {
    entries: noteEntries.slice(0, clampedLimit)
  };
}

function deriveTitleFromBody(fileName, bodyText) {
  const lines = String(bodyText).split(/\r?\n/);
  for (const line of lines) {
    const trimmed = line.trim();
    if (trimmed.startsWith("# ")) {
      return trimmed.slice(2).trim() || deriveDisplayTitle(fileName);
    }
  }

  return deriveDisplayTitle(fileName);
}

module.exports = {
  FILES_LIST_LIMIT_MAX,
  FilesSurfaceError,
  createFolder,
  deleteEntry,
  listEntries,
  listRecentNotes,
  readNote,
  renameEntry
};
