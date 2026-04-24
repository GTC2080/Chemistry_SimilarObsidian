import { useCallback, useEffect, useLayoutEffect, useRef, useState } from "react";
import { createPortal } from "react-dom";

import type { ContextMenuPosition, ContextSubmenu, MarkdownContextMenuProps } from "./contextMenuTypes";
import {
  VIEWPORT_PADDING,
  SUBMENU_GAP,
  SUBMENU_WIDTH,
  PARAGRAPH_SUBMENU_TOP,
  PARAGRAPH_SUBMENU_HEIGHT,
  INSERT_SUBMENU_TOP,
  INSERT_SUBMENU_HEIGHT,
} from "./contextMenuTypes";
import { useContextMenuActions } from "./useContextMenuActions";
import {
  ContextMenuButton,
  ContextIconButton,
  ContextFormatButton,
  ContextSubmenuButton,
} from "./ContextMenuButtons";
import { useT } from "../../i18n";

export type { ContextMenuPosition } from "./contextMenuTypes";

export default function MarkdownContextMenu({
  editor,
  position,
  activeDiscipline,
  onClose,
}: MarkdownContextMenuProps) {
  const t = useT();
  const contextMenuRef = useRef<HTMLDivElement | null>(null);
  const [contextSubmenu, setContextSubmenu] = useState<ContextSubmenu>(null);
  const [resolvedPosition, setResolvedPosition] = useState<ContextMenuPosition | null>(position);
  const submenuTimeoutRef = useRef<ReturnType<typeof setTimeout> | null>(null);

  const cancelSubmenuClose = useCallback(() => {
    if (submenuTimeoutRef.current !== null) {
      clearTimeout(submenuTimeoutRef.current);
      submenuTimeoutRef.current = null;
    }
  }, []);

  const scheduleSubmenuClose = useCallback(() => {
    cancelSubmenuClose();
    submenuTimeoutRef.current = setTimeout(() => {
      setContextSubmenu(null);
      submenuTimeoutRef.current = null;
    }, 150);
  }, [cancelSubmenuClose]);

  const closeMenu = useCallback(() => {
    cancelSubmenuClose();
    setContextSubmenu(null);
    onClose();
  }, [onClose, cancelSubmenuClose]);

  const { runEditorContextAction, runFormatAction, runParagraphAction, runInsertAction } =
    useContextMenuActions(editor, closeMenu, activeDiscipline);

  // Sync position from props
  useEffect(() => { setResolvedPosition(position); }, [position]);

  // Cleanup timer on unmount
  useEffect(() => () => cancelSubmenuClose(), [cancelSubmenuClose]);

  // Global listeners to close menu
  useEffect(() => {
    if (!position) return;

    const handlePointerDown = (event: PointerEvent) => {
      const menuEl = contextMenuRef.current;
      if (!menuEl || !menuEl.contains(event.target as Node)) closeMenu();
    };
    const handleWindowContextMenu = (event: MouseEvent) => {
      const menuEl = contextMenuRef.current;
      if (!menuEl || !menuEl.contains(event.target as Node)) closeMenu();
      else event.preventDefault();
    };
    const handleKeyDown = (event: KeyboardEvent) => {
      if (event.key === "Escape") closeMenu();
    };

    window.addEventListener("pointerdown", handlePointerDown);
    window.addEventListener("contextmenu", handleWindowContextMenu);
    window.addEventListener("scroll", closeMenu, true);
    window.addEventListener("resize", closeMenu);
    window.addEventListener("keydown", handleKeyDown);

    return () => {
      window.removeEventListener("pointerdown", handlePointerDown);
      window.removeEventListener("contextmenu", handleWindowContextMenu);
      window.removeEventListener("scroll", closeMenu, true);
      window.removeEventListener("resize", closeMenu);
      window.removeEventListener("keydown", handleKeyDown);
    };
  }, [position, closeMenu]);

  // Clamp position to viewport
  useLayoutEffect(() => {
    if (!resolvedPosition) return;
    const menuEl = contextMenuRef.current;
    if (!menuEl) return;

    const rect = menuEl.getBoundingClientRect();
    const maxX = window.innerWidth - rect.width - VIEWPORT_PADDING;
    const maxY = window.innerHeight - rect.height - VIEWPORT_PADDING;
    const nextX = Math.max(VIEWPORT_PADDING, Math.min(resolvedPosition.x, maxX));
    const nextY = Math.max(VIEWPORT_PADDING, Math.min(resolvedPosition.y, maxY));

    if (nextX !== resolvedPosition.x || nextY !== resolvedPosition.y) {
      setResolvedPosition({ x: nextX, y: nextY });
    }
  }, [resolvedPosition, contextSubmenu]);

  const getSubmenuStyle = useCallback((baseTop: number, submenuHeight: number) => {
    const menuRect = contextMenuRef.current?.getBoundingClientRect();
    const menuWidth = menuRect?.width ?? 244;
    const menuTop = resolvedPosition?.y ?? 0;
    const menuLeft = resolvedPosition?.x ?? 0;

    const openToLeft = menuLeft + menuWidth + SUBMENU_GAP + SUBMENU_WIDTH > window.innerWidth - VIEWPORT_PADDING;
    const left = openToLeft ? -(SUBMENU_WIDTH + SUBMENU_GAP) : menuWidth + SUBMENU_GAP;
    const minTop = VIEWPORT_PADDING - menuTop;
    const maxTop = window.innerHeight - submenuHeight - VIEWPORT_PADDING - menuTop;
    const top = Math.max(minTop, Math.min(baseTop, maxTop));

    return { left: `${left}px`, top: `${top}px` };
  }, [resolvedPosition]);

  if (!resolvedPosition) return null;

  return createPortal(
    <div className="fixed z-[999]" style={{ left: `${resolvedPosition.x}px`, top: `${resolvedPosition.y}px` }}>
      <div
        ref={contextMenuRef}
        className="relative min-w-[244px] rounded-xl border border-[#343434] bg-[rgba(18,18,18,0.96)] p-2 shadow-2xl backdrop-blur-xl"
        onMouseLeave={scheduleSubmenuClose}
      >
        <div className="grid grid-cols-4 gap-1">
          <ContextIconButton label={t("contextMenu.cut")} title={t("contextMenu.cut")} onClick={() => { void runEditorContextAction("cut"); }}>
            <svg className="w-4 h-4" viewBox="0 0 24 24" fill="none" stroke="currentColor" strokeWidth="1.8" strokeLinecap="round" strokeLinejoin="round">
              <circle cx="6" cy="6" r="3" /><circle cx="6" cy="18" r="3" />
              <line x1="20" y1="4" x2="8.12" y2="15.88" /><line x1="14.47" y1="14.48" x2="20" y2="20" /><line x1="8.12" y1="8.12" x2="12" y2="12" />
            </svg>
          </ContextIconButton>
          <ContextIconButton label={t("contextMenu.copy")} title={t("contextMenu.copy")} onClick={() => { void runEditorContextAction("copy"); }}>
            <svg className="w-4 h-4" viewBox="0 0 24 24" fill="none" stroke="currentColor" strokeWidth="1.8" strokeLinecap="round" strokeLinejoin="round">
              <rect x="9" y="9" width="11" height="11" rx="2" /><path d="M5 15V5a2 2 0 0 1 2-2h10" />
            </svg>
          </ContextIconButton>
          <ContextIconButton label={t("contextMenu.paste")} title={t("contextMenu.paste")} onClick={() => { void runEditorContextAction("paste"); }}>
            <svg className="w-4 h-4" viewBox="0 0 24 24" fill="none" stroke="currentColor" strokeWidth="1.8" strokeLinecap="round" strokeLinejoin="round">
              <path d="M8 4h8" /><rect x="6" y="2" width="12" height="20" rx="2" /><path d="M9 12h6M9 16h6M10 7h4" />
            </svg>
          </ContextIconButton>
          <ContextIconButton label={t("contextMenu.delete")} title={t("contextMenu.delete")} onClick={() => { void runEditorContextAction("delete"); }}>
            <svg className="w-4 h-4" viewBox="0 0 24 24" fill="none" stroke="currentColor" strokeWidth="1.8" strokeLinecap="round" strokeLinejoin="round">
              <polyline points="3 6 5 6 21 6" /><path d="M8 6V4a2 2 0 0 1 2-2h4a2 2 0 0 1 2 2v2" /><path d="M19 6l-1 14a2 2 0 0 1-2 2H8a2 2 0 0 1-2-2L5 6" />
            </svg>
          </ContextIconButton>
        </div>

        <div className="my-2 h-px bg-[#2A2A2A]" />

        <div className="grid grid-cols-4 gap-1">
          <ContextFormatButton label="B" active={editor.isActive("bold")} onClick={() => runFormatAction("bold")} />
          <ContextFormatButton label="I" active={editor.isActive("italic")} onClick={() => runFormatAction("italic")} italic />
          <ContextFormatButton label="<>" active={editor.isActive("code")} onClick={() => runFormatAction("code")} mono />
          <ContextFormatButton label={t("contextMenu.link")} onClick={() => runInsertAction("link")} />
          <ContextFormatButton label="❝" active={editor.isActive("blockquote")} onClick={() => runFormatAction("blockquote")} />
          <ContextFormatButton label="•" active={editor.isActive("bulletList")} onClick={() => runFormatAction("bulletList")} />
          <ContextFormatButton label="1." active={editor.isActive("orderedList")} onClick={() => runFormatAction("orderedList")} />
          <ContextFormatButton label="☑" active={editor.isActive("taskList")} onClick={() => runFormatAction("taskList")} />
        </div>

        <div className="my-2 h-px bg-[#2A2A2A]" />

        <ContextMenuButton label={t("contextMenu.undo")} disabled={!editor.can().undo()} onClick={() => { void runEditorContextAction("undo"); }} />
        <ContextMenuButton label={t("contextMenu.redo")} disabled={!editor.can().redo()} onClick={() => { void runEditorContextAction("redo"); }} />
        <ContextMenuButton label={t("contextMenu.selectAll")} onClick={() => { void runEditorContextAction("selectAll"); }} />
        <ContextSubmenuButton label={t("contextMenu.paragraph")} active={contextSubmenu === "paragraph"} onHover={() => { cancelSubmenuClose(); setContextSubmenu("paragraph"); }} />
        <ContextSubmenuButton label={t("contextMenu.insert")} active={contextSubmenu === "insert"} onHover={() => { cancelSubmenuClose(); setContextSubmenu("insert"); }} />

        {contextSubmenu === "paragraph" && (
          <div
            className="absolute min-w-[180px] rounded-xl border border-[#343434] bg-[rgba(18,18,18,0.98)] p-1.5 shadow-2xl"
            style={getSubmenuStyle(PARAGRAPH_SUBMENU_TOP, PARAGRAPH_SUBMENU_HEIGHT)}
            onMouseEnter={cancelSubmenuClose}
          >
            <ContextMenuButton label={t("contextMenu.body")} onClick={() => runParagraphAction("paragraph")} />
            <ContextMenuButton label={t("contextMenu.h1")} onClick={() => runParagraphAction("h1")} />
            <ContextMenuButton label={t("contextMenu.h2")} onClick={() => runParagraphAction("h2")} />
            <ContextMenuButton label={t("contextMenu.h3")} onClick={() => runParagraphAction("h3")} />
            <ContextMenuButton label={t("contextMenu.blockquote")} onClick={() => runParagraphAction("blockquote")} />
            <ContextMenuButton label={t("contextMenu.codeBlock")} onClick={() => runParagraphAction("codeBlock")} />
            <ContextMenuButton label={t("contextMenu.unorderedList")} onClick={() => runParagraphAction("bulletList")} />
            <ContextMenuButton label={t("contextMenu.orderedList")} onClick={() => runParagraphAction("orderedList")} />
            <ContextMenuButton label={t("contextMenu.taskList")} onClick={() => runParagraphAction("taskList")} />
          </div>
        )}

        {contextSubmenu === "insert" && (
          <div
            className="absolute min-w-[196px] rounded-xl border border-[#343434] bg-[rgba(18,18,18,0.98)] p-1.5 shadow-2xl"
            style={getSubmenuStyle(INSERT_SUBMENU_TOP, INSERT_SUBMENU_HEIGHT)}
            onMouseEnter={cancelSubmenuClose}
          >
            <ContextMenuButton label={t("contextMenu.hr")} onClick={() => runInsertAction("hr")} />
            <ContextMenuButton label={t("contextMenu.codeTemplate")} onClick={() => runInsertAction("codeFence")} />
            <ContextMenuButton label={t("contextMenu.markdownLink")} onClick={() => runInsertAction("link")} />
            <ContextMenuButton label={t("contextMenu.wikiLink")} onClick={() => runInsertAction("wikiLink")} />
            <ContextMenuButton label={t("contextMenu.table")} onClick={() => runInsertAction("table")} />
            <ContextMenuButton label={t("contextMenu.inlineMath")} onClick={() => runInsertAction("inlineMath")} />
            <ContextMenuButton label={t("contextMenu.blockMath")} onClick={() => runInsertAction("blockMath")} />
            <ContextMenuButton label={t("contextMenu.database")} onClick={() => runInsertAction("database")} />
            {activeDiscipline === "chemistry" && (
              <>
                <ContextMenuButton label={t("contextMenu.stoichiometry")} onClick={() => runInsertAction("stoichiometry")} />
                <ContextMenuButton label={t("contextMenu.chemdraw")} onClick={() => runInsertAction("chemdraw")} />
              </>
            )}
          </div>
        )}
      </div>
    </div>,
    document.body,
  );
}
