import { memo, useState } from "react";

// ===== Data Structure =====

/** 标签树节点（由 Rust 后端 get_tag_tree 命令返回） */
export interface TagTreeNode {
  name: string;
  fullPath: string;
  count: number;
  children: TagTreeNode[];
}

// ===== Component =====

export const TagTreeItem = memo(function TagTreeItem({
  node, depth, onSelectTag, selectedTag,
}: {
  node: TagTreeNode; depth: number; onSelectTag: (tag: string) => void; selectedTag: string | null;
}) {
  const [expanded, setExpanded] = useState(depth === 0);
  const hasChildren = node.children.length > 0;
  const isSelected = selectedTag === node.fullPath;

  return (
    <div>
      <button
        type="button"
        onClick={() => { if (hasChildren) setExpanded(p => !p); onSelectTag(node.fullPath); }}
        className="w-full text-left py-[6px] rounded-[10px] text-[13px]
          transition-colors duration-150 cursor-pointer flex items-center gap-1.5
          hover:bg-[var(--sidebar-hover)]"
        style={{
          paddingLeft: `${10 + depth * 14}px`, paddingRight: 10,
          background: isSelected ? "rgba(10,132,255,0.12)" : "transparent",
        }}
      >
        {hasChildren ? (
          <svg className="w-3 h-3 shrink-0 transition-transform duration-200"
            style={{ color: "var(--text-quaternary)", transform: expanded ? "rotate(90deg)" : "rotate(0deg)" }}
            viewBox="0 0 24 24" fill="none" stroke="currentColor" strokeWidth="2"
            strokeLinecap="round" strokeLinejoin="round">
            <polyline points="9 18 15 12 9 6" />
          </svg>
        ) : <span className="w-3 shrink-0" />}
        <span className="w-[6px] h-[6px] rounded-full shrink-0"
          style={{ background: isSelected ? "var(--accent)" : "var(--text-quaternary)" }} />
        <span className="truncate flex-1" style={{
          color: isSelected ? "var(--text-primary)" : "var(--text-secondary)",
          fontWeight: isSelected ? 500 : 400,
        }}>
          {node.name}
        </span>
        {node.count > 0 && (
          <span className="text-[10px] tabular-nums" style={{ color: "var(--text-quaternary)" }}>
            {node.count}
          </span>
        )}
      </button>
      {hasChildren && expanded && (
        <div>
          {node.children.map(child => (
            <TagTreeItem key={child.fullPath} node={child} depth={depth + 1}
              onSelectTag={onSelectTag} selectedTag={selectedTag} />
          ))}
        </div>
      )}
    </div>
  );
});
