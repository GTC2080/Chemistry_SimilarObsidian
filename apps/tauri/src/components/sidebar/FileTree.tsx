import { memo } from "react";
import type { MouseEvent } from "react";
import type { FileTreeNode, NoteInfo } from "../../types";
import { useT } from "../../i18n";
import { useFileTreeDragDrop } from "../../hooks/useFileTreeDragDrop";
import { useInlineRename } from "../../hooks/useInlineRename";
import FileIcon from "./FileIcon";

export interface FileTreeContextTarget {
  isFolder: boolean;
  label: string;
  relativePath: string;
  note: NoteInfo | null;
}

// ===== Component =====

export const FileTreeItem = memo(function FileTreeItem({
  node, depth, activeNoteId, expandedPaths, onToggleExpanded, onSelectNote, onOpenContextMenu, onMoveToFolder, onInlineRename,
}: {
  node: FileTreeNode;
  depth: number;
  activeNoteId: string | null;
  expandedPaths: Set<string>;
  onToggleExpanded: (relativePath: string) => void;
  onSelectNote: (note: NoteInfo) => void;
  onOpenContextMenu: (e: MouseEvent, target: FileTreeContextTarget) => void;
  onMoveToFolder: (sourceRelativePath: string, destFolderRelativePath: string) => void;
  onInlineRename: (sourceRelativePath: string, newName: string) => void;
}) {
  const t = useT();
  const expanded = node.isFolder && expandedPaths.has(node.relativePath);

  const {
    renaming,
    renameValue,
    renameInputRef,
    setRenameValue,
    beginRename,
    commitRename,
    cancelRename,
  } = useInlineRename({
    fullName: node.fullName,
    relativePath: node.relativePath,
    onInlineRename,
  });

  const {
    dragOver,
    handleDragStart,
    handleDragEnd,
    handleDragOver,
    handleDragEnter,
    handleDragLeave,
    handleDrop,
  } = useFileTreeDragDrop({
    relativePath: node.relativePath,
    isFolder: node.isFolder,
    renaming,
    onMoveToFolder,
  });

  if (node.isFolder) {
    const fileCount = node.fileCount;
    return (
      <div>
        <div
          role="button"
          tabIndex={0}
          draggable={!renaming}
          onDragStart={handleDragStart}
          onDragEnd={handleDragEnd}
          onDragOver={handleDragOver}
          onDragEnter={handleDragEnter}
          onDragLeave={handleDragLeave}
          onDrop={handleDrop}
          onClick={() => { if (!renaming) onToggleExpanded(node.relativePath); }}
          onKeyDown={e => {
            if (renaming) return;
            if (e.key === "Enter" || e.key === " ") { e.preventDefault(); onToggleExpanded(node.relativePath); }
          }}
          onContextMenu={e => {
            e.preventDefault();
            onOpenContextMenu(e, {
              isFolder: true,
              label: node.name,
              relativePath: node.relativePath,
              note: null,
            });
          }}
          className="w-full text-left py-[6px] rounded-[10px] text-[13px]
            transition-colors duration-150 cursor-pointer flex items-center gap-1.5
            hover:bg-[var(--sidebar-hover)]"
          style={{
            paddingLeft: `${10 + depth * 14}px`,
            paddingRight: 10,
            background: dragOver ? "rgba(10,132,255,0.18)" : undefined,
            outline: dragOver ? "2px dashed rgba(10,132,255,0.6)" : "none",
            outlineOffset: "-2px",
            borderRadius: 10,
          }}
        >
          <svg
            className="w-3 h-3 shrink-0 transition-transform duration-200"
            style={{
              color: "var(--text-quaternary)",
              transform: expanded ? "rotate(90deg)" : "rotate(0deg)",
            }}
            viewBox="0 0 24 24" fill="none" stroke="currentColor" strokeWidth="2"
            strokeLinecap="round" strokeLinejoin="round"
          >
            <polyline points="9 18 15 12 9 6" />
          </svg>
          <svg className="w-[15px] h-[15px] shrink-0"
            style={{ color: dragOver ? "rgba(10,132,255,0.85)" : expanded ? "rgba(10,132,255,0.6)" : "var(--text-quaternary)" }}
            viewBox="0 0 24 24" fill="none" stroke="currentColor" strokeWidth="1.5"
            strokeLinecap="round" strokeLinejoin="round">
            <path d="M22 19a2 2 0 0 1-2 2H4a2 2 0 0 1-2-2V5a2 2 0 0 1 2-2h5l2 3h9a2 2 0 0 1 2 2z" />
          </svg>
          {renaming ? (
            <>
              <label htmlFor={`rename-folder-${node.relativePath}`} className="sr-only">{t("fileTree.renameFolder")}</label>
              <input
                id={`rename-folder-${node.relativePath}`}
                ref={renameInputRef}
                value={renameValue}
                aria-label={t("fileTree.renameFolder")}
                title={t("fileTree.renameFolder")}
                placeholder={t("fileTree.enterNewName")}
                onChange={e => setRenameValue(e.target.value)}
                onClick={e => e.stopPropagation()}
                onDoubleClick={e => e.stopPropagation()}
                onBlur={commitRename}
                onKeyDown={e => {
                  if (e.key === "Enter") {
                    e.preventDefault();
                    commitRename();
                  } else if (e.key === "Escape") {
                    e.preventDefault();
                    cancelRename();
                  }
                }}
                className="flex-1 bg-transparent text-[13px] outline-none border-b border-white/25"
                style={{ color: "var(--text-primary)", borderBottomColor: "var(--separator)" }}
              />
            </>
          ) : (
            <span
              className="truncate flex-1"
              style={{ color: dragOver ? "rgba(10,132,255,0.9)" : "var(--text-secondary)", fontWeight: 500 }}
              onDoubleClick={e => {
                e.stopPropagation();
                beginRename();
              }}
            >
              {node.name}
            </span>
          )}
          <span className="text-[10px] tabular-nums" style={{ color: "var(--text-quaternary)" }}>
            {fileCount}
          </span>
        </div>
        {/* 子节点由 SidebarFilesPanel 的虚拟化扁平列表渲染，此处不再递归 */}
      </div>
    );
  }

  // File node
  const note = node.note!;
  const isActive = activeNoteId === note.id;

  return (
    <div
      role="button"
      tabIndex={0}
      draggable={!renaming}
      onDragStart={handleDragStart}
      onDragEnd={handleDragEnd}
      onClick={() => { if (!renaming) onSelectNote(note); }}
      onKeyDown={e => {
        if (renaming) return;
        if (e.key === "Enter" || e.key === " ") { e.preventDefault(); onSelectNote(note); }
      }}
      onContextMenu={e => {
        e.preventDefault();
        onOpenContextMenu(e, {
          isFolder: false,
          label: note.name,
          relativePath: node.relativePath,
          note,
        });
      }}
      className="w-full text-left py-[6px] rounded-[10px] text-[13px]
        transition-colors duration-150 cursor-pointer flex items-center gap-2 relative
        hover:bg-[var(--sidebar-hover)]"
      style={{
        paddingLeft: `${24 + depth * 14}px`, paddingRight: 10,
        background: isActive ? "rgba(10,132,255,0.12)" : "transparent",
      }}
    >
      {isActive && (
        <div className="absolute left-[3px] top-1/2 -translate-y-1/2 w-[3px] h-[14px] rounded-full"
          style={{ background: "var(--accent)", boxShadow: "0 0 6px rgba(10,132,255,0.4)" }} />
      )}
      <FileIcon ext={note.file_extension} active={isActive} />
      {renaming ? (
        <>
          <label htmlFor={`rename-file-${node.relativePath}`} className="sr-only">{t("fileTree.renameFile")}</label>
          <input
            id={`rename-file-${node.relativePath}`}
            ref={renameInputRef}
            value={renameValue}
            aria-label={t("fileTree.renameFile")}
            title={t("fileTree.renameFile")}
            placeholder={t("fileTree.enterNewName")}
            onChange={e => setRenameValue(e.target.value)}
            onClick={e => e.stopPropagation()}
            onDoubleClick={e => e.stopPropagation()}
            onBlur={commitRename}
            onKeyDown={e => {
              if (e.key === "Enter") {
                e.preventDefault();
                commitRename();
              } else if (e.key === "Escape") {
                e.preventDefault();
                cancelRename();
              }
            }}
            className="flex-1 bg-transparent text-[13px] outline-none border-b border-white/25"
            style={{ color: "var(--text-primary)", borderBottomColor: "var(--separator)" }}
          />
        </>
      ) : (
        <span
          className="truncate"
          style={{
            color: isActive ? "var(--text-primary)" : "var(--text-secondary)",
            fontWeight: isActive ? 500 : 400,
          }}
          onDoubleClick={e => {
            e.stopPropagation();
            beginRename();
          }}
        >
          {note.name}
          {note.file_extension !== "md" && (
            <span style={{ color: "var(--text-quaternary)", fontWeight: 400 }}>.{note.file_extension}</span>
          )}
        </span>
      )}
    </div>
  );
});
