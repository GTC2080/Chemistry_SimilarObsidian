import { createPortal } from "react-dom";
import { useRef, type RefObject } from "react";
import type { NoteInfo } from "../../types";
import { useT } from "../../i18n";
import type { FileTreeContextTarget } from "./FileTree";

export interface FileTreeContextMenuState {
  x: number;
  y: number;
  target: FileTreeContextTarget;
}

interface FileTreeContextMenuProps {
  menu: FileTreeContextMenuState | null;
  vaultPath: string;
  onClose: () => void;
  onSelectNote: (note: NoteInfo) => void;
  onCreateFile: (kind: "note" | "mol" | "paper", targetFolderRelativePath?: string) => void;
  onCreateFolder: (targetParentRelativePath?: string) => void;
  onDeleteEntry: (absolutePath: string, targetLabel: string, isFolder: boolean) => void;
  onMoveEntry: (sourceRelativePath: string, destFolderRelativePath: string) => void;
  onRenameEntry: (sourceRelativePath: string, currentFullName: string, isFolder: boolean) => void;
  onCopyPath: (path: string) => void;
  contextMenuRef: RefObject<HTMLDivElement | null>;
}

function toAbsolutePath(vaultPath: string, relativePath: string): string {
  const normalizedVault = vaultPath.replace(/[\\/]+$/, "");
  return `${normalizedVault}/${relativePath}`;
}

function getParentRelativePath(relativePath: string): string {
  const normalized = relativePath.replace(/\\/g, "/");
  const idx = normalized.lastIndexOf("/");
  return idx >= 0 ? normalized.slice(0, idx) : "";
}

function currentTargetName(target: FileTreeContextTarget): string {
  if (!target.note) return target.label;
  return target.note.id.replace(/\\/g, "/").split("/").pop() ?? target.label;
}

const menuItemClass = "w-full text-left px-2.5 py-1.5 rounded-md text-[12px] leading-5 transition-colors hover:bg-[var(--menu-hover)]";

function MenuItem({ label, onClick, danger }: { label: string; onClick: () => void; danger?: boolean }) {
  return (
    <button
      type="button"
      className={menuItemClass}
      style={{ color: danger ? "rgba(255,75,75,0.95)" : "var(--text-secondary)" }}
      onClick={onClick}
    >
      {label}
    </button>
  );
}

export default function FileTreeContextMenu({
  menu,
  vaultPath,
  onClose,
  onSelectNote,
  onCreateFile,
  onCreateFolder,
  onDeleteEntry,
  onMoveEntry,
  onRenameEntry,
  onCopyPath,
  contextMenuRef,
}: FileTreeContextMenuProps) {
  const t = useT();
  const localRef = useRef<HTMLDivElement | null>(null);
  const menuRef = contextMenuRef ?? localRef;

  if (!menu) return null;

  const { target } = menu;
  const folderPath = target.isFolder ? target.relativePath : getParentRelativePath(target.relativePath);

  const createItems: { label: string; action: () => void }[] = [
    { label: target.isFolder ? t("fileTree.newNoteHere") : t("fileTree.newNoteSibling"), action: () => onCreateFile("note", folderPath) },
    { label: target.isFolder ? t("fileTree.newMolHere") : t("fileTree.newMolSibling"), action: () => onCreateFile("mol", folderPath) },
    { label: target.isFolder ? t("fileTree.newPaperHere") : t("fileTree.newPaperSibling"), action: () => onCreateFile("paper", folderPath) },
    { label: target.isFolder ? t("fileTree.newFolderHere") : t("fileTree.newFolderSibling"), action: () => onCreateFolder(folderPath) },
  ];

  return createPortal(
    <>
      <div
        className="fixed inset-0 z-[9999]"
        style={{ background: "transparent" }}
        onPointerDown={onClose}
        onContextMenu={e => {
          e.preventDefault();
          onClose();
        }}
      />
      <div
        ref={menuRef}
        className="fixed z-[10000] w-[220px] rounded-lg p-1"
        style={{
          left: `${Math.max(8, Math.min(menu.x, window.innerWidth - 220))}px`,
          top: `${Math.max(8, Math.min(menu.y, window.innerHeight - 360))}px`,
          background: "var(--menu-bg)",
          border: "1px solid var(--separator-light)",
          boxShadow: "0 10px 28px rgba(0,0,0,0.35)",
        }}
        onClick={e => e.stopPropagation()}
      >
        {!target.isFolder && target.note && (
          <MenuItem label={t("fileTree.open")} onClick={() => { onSelectNote(target.note!); onClose(); }} />
        )}
        {target.relativePath !== "" && (
          <MenuItem
            label={t("fileTree.rename")}
            onClick={() => { onRenameEntry(target.relativePath, currentTargetName(target), target.isFolder); onClose(); }}
          />
        )}

        {createItems.map(item => (
          <MenuItem key={item.label} label={item.label} onClick={() => { item.action(); onClose(); }} />
        ))}

        <MenuItem
          label={t("fileTree.copyPath")}
          onClick={() => { onCopyPath(target.note?.path ?? toAbsolutePath(vaultPath, target.relativePath)); onClose(); }}
        />

        {target.relativePath.includes("/") && (
          <MenuItem label={t("fileTree.moveToRoot")} onClick={() => { onMoveEntry(target.relativePath, ""); onClose(); }} />
        )}

        {target.relativePath !== "" && (
          <>
            <div className="my-1 h-px" style={{ background: "var(--separator-light)" }} />
            <MenuItem
              label={target.isFolder ? t("fileTree.deleteFolder") : t("fileTree.deleteFile")}
              onClick={() => {
                onDeleteEntry(target.note?.path ?? toAbsolutePath(vaultPath, target.relativePath), target.label, target.isFolder);
                onClose();
              }}
              danger
            />
          </>
        )}
      </div>
    </>,
    document.body
  );
}
