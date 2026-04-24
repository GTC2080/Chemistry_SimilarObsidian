import { useState, useEffect, useCallback, useRef, lazy, Suspense } from "react";
import { useEditor, EditorContent } from "@tiptap/react";
import type { Editor } from "@tiptap/core";
import type { Node as PmNode } from "@tiptap/pm/model";
import "katex/dist/katex.min.css";
import { useDebounce } from "../hooks/useDebounce";
import { useMarkdownEditorExtensions } from "../hooks/useMarkdownEditorExtensions";
import { applyEditorContentSafely } from "./markdown-editor/editorContentUtils";
import MarkdownContextMenu, { type ContextMenuPosition } from "./markdown-editor/MarkdownContextMenu";
import BubbleMenuBar from "./markdown-editor/BubbleMenuBar";
import MathEditor from "./MathEditor";
import type { DisciplineProfile } from "./settings/settingsTypes";
import { useT } from "../i18n";

const ChemDrawModal = lazy(() => import("./chem-editor/ChemDrawModal"));
import ChemEditorLoading from "./chem-editor/ChemEditorLoading";

interface MarkdownEditorProps {
  initialContent: string;
  onSave: (content: string) => void;
  onContentChange?: (content: string) => void;
  vaultPath: string;
  fontFamily?: string;
  enableScientific?: boolean;
  activeDiscipline?: DisciplineProfile;
}

export default function MarkdownEditor({
  initialContent,
  onSave,
  onContentChange,
  vaultPath,
  fontFamily,
  enableScientific = true,
  activeDiscipline = "chemistry",
}: MarkdownEditorProps) {
  const t = useT();
  const tRef = useRef(t);
  tRef.current = t;
  const editorRef = useRef<Editor | null>(null);
  const editorSurfaceRef = useRef<HTMLDivElement | null>(null);
  const [mathEdit, setMathEdit] = useState<{
    latex: string;
    isBlock: boolean;
    pos: number;
    rect: DOMRect | null;
  } | null>(null);
  const [contextMenu, setContextMenu] = useState<ContextMenuPosition | null>(null);
  const [chemModalOpen, setChemModalOpen] = useState(false);
  const debouncedSave = useDebounce((md: string) => {
    onSave(md);
  }, 400);

  // Throttle liveContent updates to avoid re-rendering the whole workspace
  // on every keystroke. The semantic resonance hook has its own debounce on top.
  const debouncedContentChange = useDebounce((md: string) => {
    onContentChange?.(md);
  }, 1500);

  // Listen for activity bar "Insert into Note" trigger
  useEffect(() => {
    const handler = () => setChemModalOpen(true);
    window.addEventListener("open-chemdraw-modal", handler);
    return () => window.removeEventListener("open-chemdraw-modal", handler);
  }, []);

  const openContextMenu = useCallback((x: number, y: number) => {
    const menuWidth = 176;
    const menuHeight = 228;
    setContextMenu({
      x: Math.max(8, Math.min(x, window.innerWidth - menuWidth - 8)),
      y: Math.max(8, Math.min(y, window.innerHeight - menuHeight - 8)),
    });
  }, []);

  const handleMathClick = useCallback(
    (node: PmNode, pos: number, isBlock: boolean) => {
      // Find the DOM node for this position to get its bounding rect
      const ed = editorRef.current;
      let rect: DOMRect | null = null;
      if (ed) {
        try {
          const dom = ed.view.nodeDOM(pos);
          if (dom instanceof HTMLElement) {
            rect = dom.getBoundingClientRect();
          }
        } catch { /* fallback: no rect */ }
      }
      setMathEdit({ latex: node.attrs.latex ?? "", isBlock, pos, rect });
    },
    [],
  );

  const extensions = useMarkdownEditorExtensions({
    vaultPath,
    enableScientific,
    activeDiscipline,
    tRef,
    onMathClick: handleMathClick,
  });

  const editor = useEditor({
    extensions,
    content: "",
    onCreate({ editor }) {
      applyEditorContentSafely(editor, initialContent, enableScientific);
    },
    onUpdate({ editor }) {
      const md = (editor.storage as any).markdown?.getMarkdown?.() ?? "";
      debouncedContentChange(md);
      debouncedSave(md);
    },
  });

  // Keep ref in sync
  useEffect(() => {
    editorRef.current = editor;
  }, [editor]);

  // NOTE: "open-chemdraw-modal" listener already registered above (line 63-66)

  // Sync external content changes
  useEffect(() => {
    if (!editor || editor.isDestroyed) return;
    const current = (editor.storage as any).markdown?.getMarkdown?.() ?? "";
    if (current !== initialContent) {
      applyEditorContentSafely(editor, initialContent, enableScientific);
    }
  }, [initialContent, editor, enableScientific]);

  const handleMathConfirm = useCallback(
    (newLatex: string) => {
      if (!editor || !mathEdit) return;
      const { pos, isBlock } = mathEdit;
      const nodeType = isBlock
        ? editor.schema.nodes.blockMath
        : editor.schema.nodes.inlineMath;
      if (!nodeType) return;
      editor
        .chain()
        .focus()
        .command(({ tr }) => {
          tr.replaceWith(pos, pos + 1, nodeType.create({ latex: newLatex }));
          return true;
        })
        .run();
      setMathEdit(null);
    },
    [editor, mathEdit],
  );

  const insertSmiles = useCallback((smiles: string) => {
    editor?.chain().focus()
      .insertContent("```smiles\n" + smiles + "\n```\n")
      .run();
    setChemModalOpen(false);
  }, [editor]);

  useEffect(() => {
    const surface = editorSurfaceRef.current;
    if (!surface) return;

    const handleNativeContextMenu = (event: MouseEvent) => {
      event.preventDefault();
      event.stopPropagation();
      openContextMenu(event.clientX, event.clientY);
    };

    surface.addEventListener("contextmenu", handleNativeContextMenu, true);

    return () => {
      surface.removeEventListener("contextmenu", handleNativeContextMenu, true);
    };
  }, [openContextMenu]);

  if (!editor) return null;

  return (
    <div className="relative flex-1 flex flex-col h-full overflow-hidden">
      {/* Bubble Menu */}
      <BubbleMenuBar editor={editor} activeDiscipline={activeDiscipline} />

      {/* Editor */}
      <div
        ref={editorSurfaceRef}
        className="flex-1 overflow-y-auto"
      >
        <EditorContent
          editor={editor}
          className="prose-editor h-full min-h-full px-8 py-8"
          style={{
            fontFamily:
              fontFamily && fontFamily !== "System Default"
                ? fontFamily
                : undefined,
          }}
        />
      </div>

      <MarkdownContextMenu
        editor={editor}
        position={contextMenu}
        activeDiscipline={activeDiscipline}
        onClose={() => setContextMenu(null)}
      />

      {/* Math Editor Overlay */}
      {mathEdit && (
        <MathEditor
          latex={mathEdit.latex}
          isBlock={mathEdit.isBlock}
          anchorRect={mathEdit.rect}
          onConfirm={handleMathConfirm}
          onClose={() => setMathEdit(null)}
        />
      )}

      {/* ChemDraw Modal (lazy) */}
      <Suspense fallback={<ChemEditorLoading />}>
        {chemModalOpen && (
          <ChemDrawModal
            open={chemModalOpen}
            onClose={() => setChemModalOpen(false)}
            onConfirm={insertSmiles}
          />
        )}
      </Suspense>
    </div>
  );
}
