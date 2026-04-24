import { useCallback } from "react";
import type { Editor } from "@tiptap/core";
import type { DisciplineProfile } from "../settings/settingsTypes";
import type { EditorAction, FormatAction, InsertAction, ParagraphAction } from "./contextMenuTypes";

export function useContextMenuActions(
  editor: Editor,
  closeMenu: () => void,
  activeDiscipline: DisciplineProfile,
) {
  const runEditorContextAction = useCallback(
    async (action: EditorAction) => {
      closeMenu();
      editor.commands.focus();
      switch (action) {
        case "undo":
          editor.chain().focus().undo().run();
          break;
        case "redo":
          editor.chain().focus().redo().run();
          break;
        case "cut":
          document.execCommand("cut");
          break;
        case "copy":
          document.execCommand("copy");
          break;
        case "paste":
          try {
            const text = await navigator.clipboard.readText();
            if (text) {
              editor.chain().focus().insertContent(text).run();
            }
          } catch {
            document.execCommand("paste");
          }
          break;
        case "selectAll":
          editor.chain().focus().selectAll().run();
          break;
        case "delete":
          editor.chain().focus().deleteSelection().run();
          break;
      }
    },
    [editor, closeMenu],
  );

  const runFormatAction = useCallback((action: FormatAction) => {
    closeMenu();
    const chain = editor.chain().focus();
    switch (action) {
      case "bold":
        chain.toggleBold().run();
        break;
      case "italic":
        chain.toggleItalic().run();
        break;
      case "code":
        chain.toggleCode().run();
        break;
      case "blockquote":
        chain.toggleBlockquote().run();
        break;
      case "bulletList":
        chain.toggleBulletList().run();
        break;
      case "orderedList":
        chain.toggleOrderedList().run();
        break;
      case "taskList":
        chain.toggleTaskList().run();
        break;
    }
  }, [editor, closeMenu]);

  const runParagraphAction = useCallback((action: ParagraphAction) => {
    closeMenu();
    const chain = editor.chain().focus();
    switch (action) {
      case "paragraph":
        chain.setParagraph().run();
        break;
      case "h1":
        chain.toggleHeading({ level: 1 }).run();
        break;
      case "h2":
        chain.toggleHeading({ level: 2 }).run();
        break;
      case "h3":
        chain.toggleHeading({ level: 3 }).run();
        break;
      case "blockquote":
        chain.toggleBlockquote().run();
        break;
      case "codeBlock":
        chain.toggleCodeBlock().run();
        break;
      case "bulletList":
        chain.toggleBulletList().run();
        break;
      case "orderedList":
        chain.toggleOrderedList().run();
        break;
      case "taskList":
        chain.toggleTaskList().run();
        break;
    }
  }, [editor, closeMenu]);

  const runInsertAction = useCallback((action: InsertAction) => {
    closeMenu();
    editor.commands.focus();

    const selectedText = editor.state.doc.textBetween(editor.state.selection.from, editor.state.selection.to, "\n");

    switch (action) {
      case "hr":
        editor.chain().focus().setHorizontalRule().run();
        break;
      case "codeFence":
        editor.chain().focus().insertContent("```text\n\n```").run();
        break;
      case "link":
        editor.chain().focus().insertContent(selectedText ? `[${selectedText}](https://)` : "[link text](https://)").run();
        break;
      case "wikiLink":
        editor.chain().focus().insertContent(selectedText ? `[[${selectedText}]]` : "[[]]").run();
        break;
      case "table":
        editor
          .chain()
          .focus()
          .insertTable({ rows: 3, cols: 2, withHeaderRow: true })
          .run();
        break;
      case "inlineMath":
        editor.chain().focus().insertContent(selectedText ? `$${selectedText}$` : "$x$").run();
        break;
      case "blockMath":
        editor.chain().focus().insertContent("$$\n\n$$").run();
        break;
      case "database":
        editor.chain().focus().insertDatabaseBlock().run();
        break;
      case "stoichiometry":
        if (activeDiscipline === "chemistry") {
          editor.chain().focus().insertStoichiometryBlock().run();
        }
        break;
      case "chemdraw":
        window.dispatchEvent(new CustomEvent("open-chemdraw-modal"));
        break;
    }
  }, [editor, closeMenu, activeDiscipline]);

  return { runEditorContextAction, runFormatAction, runParagraphAction, runInsertAction };
}
