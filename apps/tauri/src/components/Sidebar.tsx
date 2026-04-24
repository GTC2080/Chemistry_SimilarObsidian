import { useState, useEffect, useCallback, useMemo, useRef } from "react";
import { useT } from "../i18n";
import type { MouseEvent as ReactMouseEvent, DragEvent } from "react";
import { invoke } from "@tauri-apps/api/core";
import type { FileTreeNode, NoteInfo } from "../types";
import { type FileTreeContextTarget } from "./sidebar/FileTree";
import { useSidebarTags } from "../hooks/useSidebarTags";
import { useContextMenuDismiss } from "../hooks/useContextMenuDismiss";
import FileTreeContextMenu, { type FileTreeContextMenuState } from "./sidebar/FileTreeContextMenu";
import SidebarHeader from "./sidebar/SidebarHeader";
import SidebarFilesPanel from "./sidebar/SidebarFilesPanel";
import SidebarTagsPanel from "./sidebar/SidebarTagsPanel";

interface SidebarProps {
  vaultPath: string;
  ignoredFolders: string;
  notes: NoteInfo[];
  activeNote: NoteInfo | null;
  loading: boolean;
  width: number;
  onSelectNote: (note: NoteInfo) => void;
  onCreateFile: (kind: "note" | "mol" | "paper", targetFolderRelativePath?: string) => void;
  onDeleteEntry: (absolutePath: string, targetLabel: string, isFolder: boolean) => void;
  onMoveEntry: (sourceRelativePath: string, destFolderRelativePath: string) => void;
  onRenameEntry: (sourceRelativePath: string, currentFullName: string, isFolder: boolean) => void;
  onInlineRenameEntry: (sourceRelativePath: string, newName: string) => void;
  onCreateFolder: (targetParentRelativePath?: string) => void;
}

/** 文件树面板 — 纯内容，无工具按钮 */
export default function Sidebar({
  vaultPath,
  ignoredFolders,
  notes,
  activeNote,
  loading,
  width,
  onSelectNote,
  onCreateFile,
  onDeleteEntry,
  onMoveEntry,
  onRenameEntry,
  onInlineRenameEntry,
  onCreateFolder,
}: SidebarProps) {
  const t = useT();
  const [tab, setTab] = useState<"files" | "tags">("files");
  const [newMenuOpen, setNewMenuOpen] = useState(false);
  const [contextMenu, setContextMenu] = useState<FileTreeContextMenuState | null>(null);
  const contextMenuRef = useRef<HTMLDivElement | null>(null);
  const [fileTree, setFileTree] = useState<FileTreeNode[]>([]);

  // --- Expand state persistence (keyed by vault) ---
  const expandKey = `sidebar-expanded:${vaultPath}`;
  const [expandedPaths, setExpandedPaths] = useState<Set<string>>(() => {
    try {
      const saved = localStorage.getItem(expandKey);
      if (saved) return new Set(JSON.parse(saved) as string[]);
    } catch { /* ignore */ }
    return new Set<string>();
  });

  // Persist expand state on change (debounced via a ref to avoid thrashing).
  const expandTimerRef = useRef<ReturnType<typeof setTimeout> | null>(null);
  useEffect(() => {
    if (!vaultPath) return;
    if (expandTimerRef.current) clearTimeout(expandTimerRef.current);
    expandTimerRef.current = setTimeout(() => {
      try {
        localStorage.setItem(expandKey, JSON.stringify([...expandedPaths]));
      } catch { /* localStorage may be full */ }
    }, 500);
  }, [expandedPaths, expandKey, vaultPath]);

  const toggleExpanded = useCallback((relativePath: string) => {
    setExpandedPaths(prev => {
      const next = new Set(prev);
      if (next.has(relativePath)) {
        next.delete(relativePath);
      } else {
        next.add(relativePath);
      }
      return next;
    });
  }, []);

  const { tags, tagTree, selectedTag, tagNotes, tagNotesPending, handleSelectTag } =
    useSidebarTags({ vaultPath, notes, tab });

  // 用所有 note 的 id + updated_at 计算内容指纹，确保任何变化都能触发重建
  const notesFingerprint = useMemo(() => {
    if (notes.length === 0) return "";
    // FNV-1a hash over all IDs and timestamps — covers增删改重命名
    let hash = 2166136261;
    for (const n of notes) {
      for (let i = 0; i < n.id.length; i++) {
        hash ^= n.id.charCodeAt(i);
        hash = Math.imul(hash, 16777619);
      }
      hash ^= n.updated_at;
      hash = Math.imul(hash, 16777619);
    }
    return `${notes.length}:${hash >>> 0}`;
  }, [notes]);

  useEffect(() => {
    if (!vaultPath) { setFileTree([]); return; }
    let cancelled = false;
    invoke<FileTreeNode[]>("build_file_tree", {
      vaultPath,
      ignoredFolders: ignoredFolders || "",
    })
      .then(tree => {
        if (!cancelled) setFileTree(tree);
      })
      .catch(e => {
        console.error("构建文件树失败:", e);
        if (!cancelled) setFileTree([]);
      });
    return () => {
      cancelled = true;
    };
    // eslint-disable-next-line react-hooks/exhaustive-deps
  }, [ignoredFolders, notesFingerprint, vaultPath]);

  const closeContextMenu = useCallback(() => setContextMenu(null), []);
  useContextMenuDismiss(!!contextMenu, contextMenuRef, closeContextMenu);

  const copyPathToClipboard = useCallback(async (path: string) => {
    try {
      await navigator.clipboard.writeText(path);
    } catch (e) {
      console.error("复制路径失败:", e);
    }
  }, []);

  const handleTreeContextMenu = useCallback((e: ReactMouseEvent, target: FileTreeContextTarget) => {
    setContextMenu({
      x: e.clientX,
      y: e.clientY,
      target,
    });
  }, []);

  const [rootDragOver, setRootDragOver] = useState(false);
  const rootDragCountRef = useRef(0);

  const handleRootDragOver = useCallback((e: DragEvent) => {
    e.preventDefault();
    e.dataTransfer.dropEffect = "move";
  }, []);

  const handleRootDragEnter = useCallback((e: DragEvent) => {
    e.preventDefault();
    rootDragCountRef.current++;
    if (rootDragCountRef.current === 1) {
      setRootDragOver(true);
    }
  }, []);

  const handleRootDragLeave = useCallback(() => {
    rootDragCountRef.current--;
    if (rootDragCountRef.current <= 0) {
      rootDragCountRef.current = 0;
      setRootDragOver(false);
    }
  }, []);

  const handleRootDrop = useCallback((e: DragEvent) => {
    e.preventDefault();
    rootDragCountRef.current = 0;
    setRootDragOver(false);
    const sourcePath = e.dataTransfer.getData("text/x-filetree-path");
    if (!sourcePath) return;
    if (!sourcePath.includes("/")) return;
    onMoveEntry(sourcePath, "");
  }, [onMoveEntry]);

  return (
    <aside
      className="flex flex-col select-none workspace-panel"
      style={{
        width: `var(--sidebar-drag-width, ${width}px)`,
        minWidth: `var(--sidebar-drag-width, ${width}px)`,
        margin: "0",
        borderLeft: "none",
        borderRight: "0.5px solid var(--panel-border)",
        overflow: "hidden",
      }}
    >
      <SidebarHeader
        vaultPath={vaultPath}
        tab={tab}
        tagsCount={tags.length}
        newMenuOpen={newMenuOpen}
        onToggleNewMenu={() => setNewMenuOpen(v => !v)}
        onSelectTab={next => {
          setTab(next);
          setNewMenuOpen(false);
        }}
        onCreateFile={(kind, targetFolderRelativePath) => {
          setNewMenuOpen(false);
          onCreateFile(kind, targetFolderRelativePath);
        }}
        onCreateFolder={targetParentRelativePath => {
          setNewMenuOpen(false);
          onCreateFolder(targetParentRelativePath);
        }}
      />

      {tab === "files" && notes.length > 0 && (
        <div className="px-4 pb-1">
          <span className="text-[11px] font-medium uppercase tracking-wider" style={{ color: "var(--text-quinary)" }}>
            {t("sidebar.filesCount")} <span className="ml-1 normal-case tracking-normal">{notes.length}</span>
          </span>
        </div>
      )}

      <nav
        className="flex-1 min-h-0 px-2 pb-3 pt-1 flex flex-col"
        onDragOver={handleRootDragOver}
        onDragEnter={handleRootDragEnter}
        onDragLeave={handleRootDragLeave}
        onDrop={handleRootDrop}
        onContextMenu={e => {
          if (tab !== "files") return;
          // Only trigger when clicking directly on the nav (blank area), not on child elements
          if (e.target === e.currentTarget) {
            e.preventDefault();
            handleTreeContextMenu(e, {
              isFolder: true,
              label: "",
              relativePath: "",
              note: null,
            });
          }
        }}
        style={rootDragOver ? { background: "var(--accent-soft)" } : undefined}
      >
        {tab === "files" ? (
          <SidebarFilesPanel
            loading={loading}
            notes={notes}
            vaultPath={vaultPath}
            fileTree={fileTree}
            activeNoteId={activeNote?.id ?? null}
            expandedPaths={expandedPaths}
            onToggleExpanded={toggleExpanded}
            onSelectNote={onSelectNote}
            onOpenContextMenu={handleTreeContextMenu}
            onMoveEntry={onMoveEntry}
            onInlineRenameEntry={onInlineRenameEntry}
          />
        ) : (
          !loading && (
            <div className="flex-1 overflow-y-auto">
            <SidebarTagsPanel
              tagsCount={tags.length}
              tagTree={tagTree}
              selectedTag={selectedTag}
              tagNotes={tagNotes}
              tagNotesLoading={tagNotesPending}
              activeNoteId={activeNote?.id ?? null}
              onSelectTag={handleSelectTag}
              onSelectNote={onSelectNote}
            />
            </div>
          )
        )}
      </nav>

      <FileTreeContextMenu
        menu={contextMenu}
        vaultPath={vaultPath}
        onClose={() => setContextMenu(null)}
        onSelectNote={onSelectNote}
        onCreateFile={onCreateFile}
        onCreateFolder={onCreateFolder}
        onDeleteEntry={onDeleteEntry}
        onMoveEntry={onMoveEntry}
        onRenameEntry={onRenameEntry}
        onCopyPath={path => {
          void copyPathToClipboard(path);
        }}
        contextMenuRef={contextMenuRef}
      />
    </aside>
  );
}
