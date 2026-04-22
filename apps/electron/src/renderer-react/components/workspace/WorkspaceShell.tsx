import { useMemo, useState } from "react";
import ResizeHandle from "../nexus/ResizeHandle";
import ActivityBar from "../nexus/ActivityBar";
import AppStatusBar from "../nexus/AppStatusBar";
import EditorViewport from "./EditorViewport";
import { useT } from "../../i18n";
import type { NoteRecord, TreeNode } from "../../lib/files-tree";

interface WorkspaceShellProps {
  vaultPath: string;
  tree: TreeNode[];
  notes: NoteRecord[];
  recentNotes: NoteRecord[];
  activeNote: NoteRecord | null;
  noteBody: string;
  contentLoading: boolean;
  contentError: string | null;
  searchQuery: string;
  searchResults: { relPath: string; title: string }[];
  searchLoading: boolean;
  onBackToManager: () => void;
  onSelectNote: (relPath: string) => void;
  onClearNote: () => void;
  onSearchQueryChange: (value: string) => void;
}

export default function WorkspaceShell({
  vaultPath,
  tree,
  notes,
  recentNotes,
  activeNote,
  noteBody,
  contentLoading,
  contentError,
  searchQuery,
  searchResults,
  searchLoading,
  onBackToManager,
  onSelectNote,
  onClearNote,
  onSearchQueryChange
}: WorkspaceShellProps) {
  const [sidebarWidth, setSidebarWidth] = useState(280);
  const [panel, setPanel] = useState<"files" | "search">("files");

  const handleSidebarDrag = (event: React.MouseEvent) => {
    event.preventDefault();
    const startX = event.clientX;
    const startWidth = sidebarWidth;

    function onMove(moveEvent: MouseEvent) {
      const next = Math.max(220, Math.min(420, startWidth + (moveEvent.clientX - startX)));
      setSidebarWidth(next);
    }

    function onUp() {
      window.removeEventListener("mousemove", onMove);
      window.removeEventListener("mouseup", onUp);
    }

    window.addEventListener("mousemove", onMove);
    window.addEventListener("mouseup", onUp);
  };

  const vaultName = useMemo(
    () => vaultPath.replace(/[\\/]+$/, "").split(/[\\/]/).pop() || "Vault",
    [vaultPath]
  );

  return (
    <>
      <div className="flex flex-1 min-h-0">
        <ActivityBar
          onBackToManager={onBackToManager}
          onOpenSearch={() => setPanel((current) => current === "search" ? "files" : "search")}
          activePanel={panel}
          visibleItems={["search"]}
        />

        <aside
          className="flex flex-col select-none workspace-panel"
          style={{
            width: `${sidebarWidth}px`,
            minWidth: `${sidebarWidth}px`,
            borderRight: "0.5px solid var(--panel-border)",
            overflow: "hidden"
          }}
        >
          <div className="px-3 pt-3 pb-2">
            <div className="flex items-center justify-between mb-2">
              <span className="text-[13px] font-semibold truncate mr-2 text-[var(--text-primary)]" title={vaultPath}>
                {vaultName}
              </span>
              <span className="text-[11px] text-[var(--text-quaternary)]">{notes.length}</span>
            </div>
            <div
              className="flex p-[3px] rounded-[10px]"
              style={{ background: "var(--subtle-surface)", border: "0.5px solid var(--panel-border)" }}
            >
              {(["files", "search"] as const).map((nextTab) => {
                const active = panel === nextTab;
                return (
                  <button
                    key={nextTab}
                    onClick={() => setPanel(nextTab)}
                    className="flex-1 px-2 py-[5px] rounded-[9px] text-[12px] font-medium transition-colors duration-250 cursor-pointer flex items-center justify-center gap-1.5"
                    style={{
                      background: active ? "rgba(10,132,255,0.16)" : "transparent",
                      color: active ? "var(--text-primary)" : "var(--text-tertiary)",
                      boxShadow: active
                        ? "0 1px 4px rgba(0,0,0,0.25), 0 0.5px 1px rgba(0,0,0,0.15), inset 0 0.5px 0 rgba(255,255,255,0.08)"
                        : "none"
                    }}
                  >
                    {nextTab === "files" ? "目录" : "搜索"}
                  </button>
                );
              })}
            </div>
          </div>

          {panel === "search" ? (
            <SearchSidebar
              query={searchQuery}
              results={searchResults}
              loading={searchLoading}
              onQueryChange={onSearchQueryChange}
              onSelectNote={onSelectNote}
            />
          ) : (
            <FilesSidebar tree={tree} recentNotes={recentNotes} activeRelPath={activeNote?.relPath ?? null} onSelectNote={onSelectNote} />
          )}
        </aside>

        <ResizeHandle side="left" onMouseDown={handleSidebarDrag} />

        <EditorViewport
          activeNote={activeNote}
          noteBody={noteBody}
          contentLoading={contentLoading}
          contentError={contentError}
          onCloseNote={onClearNote}
        />
      </div>

      <AppStatusBar vaultPath={vaultPath} />
    </>
  );
}

function FilesSidebar({
  tree,
  recentNotes,
  activeRelPath,
  onSelectNote
}: {
  tree: TreeNode[];
  recentNotes: NoteRecord[];
  activeRelPath: string | null;
  onSelectNote: (relPath: string) => void;
}) {
  const t = useT();

  return (
    <div className="flex-1 min-h-0 overflow-y-auto px-2 pb-4">
      {recentNotes.length > 0 && (
        <div className="px-2 py-2">
          <div className="text-[11px] font-medium uppercase tracking-wider text-[var(--text-quinary)]">Recent</div>
          <div className="mt-2 space-y-1">
            {recentNotes.slice(0, 5).map((note) => (
              <button
                key={`recent-${note.relPath}`}
                onClick={() => onSelectNote(note.relPath)}
                className="w-full text-left px-3 py-2 rounded-[10px] hover:bg-[var(--sidebar-hover)] transition-colors"
                style={{ background: activeRelPath === note.relPath ? "rgba(10,132,255,0.12)" : "transparent" }}
              >
                <div className="text-[13px] text-[var(--text-secondary)] truncate">{note.title || note.name}</div>
                <div className="text-[11px] text-[var(--text-quaternary)] truncate">{note.relPath}</div>
              </button>
            ))}
          </div>
        </div>
      )}

      <div className="px-2 py-2">
        <div className="text-[11px] font-medium uppercase tracking-wider text-[var(--text-quinary)]">{t("sidebar.files")}</div>
        <div className="mt-2 space-y-0.5">
          {tree.length === 0 ? (
            <div className="px-3 py-4 text-[12px] text-[var(--text-quaternary)]">{t("sidebar.noFiles")}</div>
          ) : tree.map((node) => (
            <TreeItem key={node.relPath} node={node} depth={0} activeRelPath={activeRelPath} onSelectNote={onSelectNote} />
          ))}
        </div>
      </div>
    </div>
  );
}

function TreeItem({
  node,
  depth,
  activeRelPath,
  onSelectNote
}: {
  node: TreeNode;
  depth: number;
  activeRelPath: string | null;
  onSelectNote: (relPath: string) => void;
}) {
  const [expanded, setExpanded] = useState(depth < 1);

  if (node.isFolder) {
    return (
      <div>
        <button
          onClick={() => setExpanded((value) => !value)}
          className="w-full text-left py-[6px] rounded-[10px] text-[13px] transition-colors duration-150 cursor-pointer flex items-center gap-1.5 hover:bg-[var(--sidebar-hover)]"
          style={{ paddingLeft: `${10 + depth * 14}px`, paddingRight: 10 }}
        >
          <svg
            className="w-3 h-3 shrink-0 transition-transform duration-200"
            style={{ color: "var(--text-quaternary)", transform: expanded ? "rotate(90deg)" : "rotate(0deg)" }}
            viewBox="0 0 24 24"
            fill="none"
            stroke="currentColor"
            strokeWidth="2"
            strokeLinecap="round"
            strokeLinejoin="round"
          >
            <polyline points="9 18 15 12 9 6" />
          </svg>
          <svg className="w-[15px] h-[15px] shrink-0" style={{ color: expanded ? "rgba(10,132,255,0.6)" : "var(--text-quaternary)" }} viewBox="0 0 24 24" fill="none" stroke="currentColor" strokeWidth="1.5" strokeLinecap="round" strokeLinejoin="round">
            <path d="M22 19a2 2 0 0 1-2 2H4a2 2 0 0 1-2-2V5a2 2 0 0 1 2-2h5l2 3h9a2 2 0 0 1 2 2z" />
          </svg>
          <span className="truncate flex-1 text-[var(--text-secondary)] font-medium">{node.name}</span>
        </button>
        {expanded && node.children.map((child) => (
          <TreeItem key={child.relPath} node={child} depth={depth + 1} activeRelPath={activeRelPath} onSelectNote={onSelectNote} />
        ))}
      </div>
    );
  }

  return (
    <button
      onClick={() => onSelectNote(node.relPath)}
      className="w-full text-left py-[6px] rounded-[10px] text-[13px] transition-colors duration-150 cursor-pointer flex items-center gap-2 relative hover:bg-[var(--sidebar-hover)]"
      style={{
        paddingLeft: `${24 + depth * 14}px`,
        paddingRight: 10,
        background: activeRelPath === node.relPath ? "rgba(10,132,255,0.12)" : "transparent"
      }}
    >
      {activeRelPath === node.relPath && (
        <div className="absolute left-[3px] top-1/2 -translate-y-1/2 w-[3px] h-[14px] rounded-full" style={{ background: "var(--accent)", boxShadow: "0 0 6px rgba(10,132,255,0.4)" }} />
      )}
      <svg className="w-[15px] h-[15px] shrink-0 text-[var(--text-quaternary)]" viewBox="0 0 24 24" fill="none" stroke="currentColor" strokeWidth="1.5" strokeLinecap="round" strokeLinejoin="round">
        <path d="M14 2H6a2 2 0 0 0-2 2v16a2 2 0 0 0 2 2h12a2 2 0 0 0 2-2V8z" />
        <polyline points="14 2 14 8 20 8" />
      </svg>
      <span className="truncate" style={{ color: activeRelPath === node.relPath ? "var(--text-primary)" : "var(--text-secondary)", fontWeight: activeRelPath === node.relPath ? 500 : 400 }}>
        {node.note?.name ?? node.name}
      </span>
    </button>
  );
}

function SearchSidebar({
  query,
  results,
  loading,
  onQueryChange,
  onSelectNote
}: {
  query: string;
  results: { relPath: string; title: string }[];
  loading: boolean;
  onQueryChange: (value: string) => void;
  onSelectNote: (relPath: string) => void;
}) {
  return (
    <div className="flex-1 min-h-0 overflow-y-auto px-3 pb-4">
      <div className="px-1 py-2">
        <input
          value={query}
          onChange={(event) => onQueryChange(event.target.value)}
          placeholder="搜索笔记..."
          className="w-full rounded-[10px] px-3 py-2 text-[13px] outline-none border-[0.5px] border-[var(--panel-border)] bg-[var(--subtle-surface)] text-[var(--text-primary)]"
        />
      </div>
      {loading ? (
        <div className="px-3 py-4 text-[12px] text-[var(--text-quaternary)]">搜索中…</div>
      ) : (
        <div className="space-y-1">
          {results.map((result) => (
            <button
              key={result.relPath}
              onClick={() => onSelectNote(result.relPath)}
              className="w-full text-left px-3 py-2 rounded-[10px] hover:bg-[var(--sidebar-hover)] transition-colors"
            >
              <div className="text-[13px] text-[var(--text-secondary)] truncate">{result.title}</div>
              <div className="text-[11px] text-[var(--text-quaternary)] truncate">{result.relPath}</div>
            </button>
          ))}
          {!results.length && query.trim() && (
            <div className="px-3 py-4 text-[12px] text-[var(--text-quaternary)]">未找到相关笔记</div>
          )}
        </div>
      )}
    </div>
  );
}
