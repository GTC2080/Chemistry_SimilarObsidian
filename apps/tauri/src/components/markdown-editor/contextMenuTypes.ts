import type { Editor } from "@tiptap/core";
import type { DisciplineProfile } from "../settings/settingsTypes";

export interface ContextMenuPosition {
  x: number;
  y: number;
}

export type ContextSubmenu = "paragraph" | "insert" | null;
export type EditorAction = "undo" | "redo" | "cut" | "copy" | "paste" | "selectAll" | "delete";
export type FormatAction = "bold" | "italic" | "code" | "blockquote" | "bulletList" | "orderedList" | "taskList";
export type ParagraphAction = "paragraph" | "h1" | "h2" | "h3" | "blockquote" | "codeBlock" | "bulletList" | "orderedList" | "taskList";
export type InsertAction = "hr" | "codeFence" | "link" | "wikiLink" | "table" | "inlineMath" | "blockMath" | "database" | "stoichiometry" | "chemdraw";

export interface MarkdownContextMenuProps {
  editor: Editor;
  position: ContextMenuPosition | null;
  activeDiscipline: DisciplineProfile;
  onClose: () => void;
}

export const VIEWPORT_PADDING = 8;
export const SUBMENU_GAP = 8;
export const PARAGRAPH_SUBMENU_TOP = 142;
export const INSERT_SUBMENU_TOP = 174;
export const PARAGRAPH_SUBMENU_HEIGHT = 292;
export const INSERT_SUBMENU_HEIGHT = 324;
export const SUBMENU_WIDTH = 196;
