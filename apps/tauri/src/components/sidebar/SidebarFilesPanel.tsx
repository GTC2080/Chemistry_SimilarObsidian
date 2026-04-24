import { memo, useRef, useMemo } from "react";
import type { MouseEvent as ReactMouseEvent } from "react";
import { useVirtualizer } from "@tanstack/react-virtual";
import type { FileTreeNode, NoteInfo } from "../../types";
import { useT } from "../../i18n";
import { FileTreeItem, type FileTreeContextTarget } from "./FileTree";

interface SidebarFilesPanelProps {
  loading: boolean;
  notes: NoteInfo[];
  vaultPath: string;
  fileTree: FileTreeNode[];
  activeNoteId: string | null;
  expandedPaths: Set<string>;
  onToggleExpanded: (relativePath: string) => void;
  onSelectNote: (note: NoteInfo) => void;
  onOpenContextMenu: (e: ReactMouseEvent, target: FileTreeContextTarget) => void;
  onMoveEntry: (sourceRelativePath: string, destFolderRelativePath: string) => void;
  onInlineRenameEntry: (sourceRelativePath: string, newName: string) => void;
}

/** 将树形结构按展开状态扁平化为可见节点列表 */
interface FlatNode {
  node: FileTreeNode;
  depth: number;
}

function flattenVisibleNodes(
  roots: FileTreeNode[],
  expandedPaths: Set<string>,
): FlatNode[] {
  const result: FlatNode[] = [];
  const stack: { node: FileTreeNode; depth: number }[] = [];

  // 倒序入栈，保证出栈时正序
  for (let i = roots.length - 1; i >= 0; i--) {
    stack.push({ node: roots[i], depth: 0 });
  }

  while (stack.length > 0) {
    const { node, depth } = stack.pop()!;
    result.push({ node, depth });

    // 只有展开的文件夹才递归子节点
    if (node.isFolder && expandedPaths.has(node.relativePath)) {
      for (let i = node.children.length - 1; i >= 0; i--) {
        stack.push({ node: node.children[i], depth: depth + 1 });
      }
    }
  }

  return result;
}

const ROW_HEIGHT = 32; // 与 py-[6px] + 内容高度对齐

export default memo(function SidebarFilesPanel({
  loading,
  notes,
  vaultPath,
  fileTree,
  activeNoteId,
  expandedPaths,
  onToggleExpanded,
  onSelectNote,
  onOpenContextMenu,
  onMoveEntry,
  onInlineRenameEntry,
}: SidebarFilesPanelProps) {
  const t = useT();
  const parentRef = useRef<HTMLDivElement>(null);

  // 扁平化可见节点列表 — expandedPaths 或 fileTree 变化时重算
  const flatNodes = useMemo(
    () => flattenVisibleNodes(fileTree, expandedPaths),
    [fileTree, expandedPaths],
  );

  const virtualizer = useVirtualizer({
    count: flatNodes.length,
    getScrollElement: () => parentRef.current,
    estimateSize: () => ROW_HEIGHT,
    overscan: 15, // 多渲染 15 行缓冲，平衡流畅度和内存
  });

  if (loading) {
    return (
      <div className="flex flex-col items-center py-12 gap-3">
        <div
          className="w-5 h-5 rounded-full border-[1.5px] animate-spin"
          style={{ borderColor: "rgba(255,255,255,0.06)", borderTopColor: "var(--accent)" }}
        />
        <p className="text-[11px]" style={{ color: "var(--text-quaternary)" }}>{t("sidebar.scanning")}</p>
      </div>
    );
  }

  if (notes.length === 0 && vaultPath) {
    return (
      <p className="text-[12px] text-center py-12" style={{ color: "var(--text-quaternary)" }}>
        {t("sidebar.noFiles")}
      </p>
    );
  }

  return (
    <div
      ref={parentRef}
      className="flex-1 overflow-y-auto"
      style={{ contain: "strict" }}
    >
      <div
        style={{
          height: `${virtualizer.getTotalSize()}px`,
          width: "100%",
          position: "relative",
        }}
      >
        {virtualizer.getVirtualItems().map((virtualRow) => {
          const { node, depth } = flatNodes[virtualRow.index];
          return (
            <div
              key={node.isFolder ? `d:${node.relativePath}` : (node.note?.id ?? virtualRow.index)}
              style={{
                position: "absolute",
                top: 0,
                left: 0,
                width: "100%",
                height: `${virtualRow.size}px`,
                transform: `translateY(${virtualRow.start}px)`,
              }}
            >
              <FileTreeItem
                node={node}
                depth={depth}
                activeNoteId={activeNoteId}
                expandedPaths={expandedPaths}
                onToggleExpanded={onToggleExpanded}
                onSelectNote={onSelectNote}
                onOpenContextMenu={onOpenContextMenu}
                onMoveToFolder={onMoveEntry}
                onInlineRename={onInlineRenameEntry}
              />
            </div>
          );
        })}
      </div>
    </div>
  );
});
