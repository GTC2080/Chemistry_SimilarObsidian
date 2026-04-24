import { useMemo } from "react";
import type { Extension } from "@tiptap/core";
import type { Node as PmNode } from "@tiptap/pm/model";
import StarterKit from "@tiptap/starter-kit";
import TaskList from "@tiptap/extension-task-list";
import TaskItem from "@tiptap/extension-task-item";
import Placeholder from "@tiptap/extension-placeholder";
import { Table } from "@tiptap/extension-table";
import { TableRow } from "@tiptap/extension-table-row";
import { TableHeader } from "@tiptap/extension-table-header";
import { TableCell } from "@tiptap/extension-table-cell";
import { Markdown } from "tiptap-markdown";
import { WikiLink } from "../editor/extensions/WikiLink";
import { TagHighlight } from "../editor/extensions/Tag";
import { createWikiLinkSuggestion } from "../editor/suggestion";
import { InlineMathWithMarkdown, BlockMathWithMarkdown, sharedKatexOptions } from "../editor/extensions/MathMarkdown";
import { DatabaseBlock } from "../editor/extensions/DatabaseNode";
import { StoichiometryBlock } from "../editor/extensions/StoichiometryNode";
import { ChemDrawCommand } from "../editor/extensions/ChemDrawCommand";
import type { DisciplineProfile } from "../components/settings/settingsTypes";

export function useMarkdownEditorExtensions(options: {
  vaultPath: string;
  enableScientific: boolean;
  activeDiscipline: DisciplineProfile;
  tRef: React.RefObject<(key: string) => string>;
  onMathClick: (node: PmNode, pos: number, isBlock: boolean) => void;
}): Extension[] {
  const { vaultPath, enableScientific, activeDiscipline, tRef, onMathClick } = options;

  return useMemo(
    () => [
      StarterKit.configure({ codeBlock: { HTMLAttributes: { class: "hljs" } } }),
      TaskList,
      TaskItem.configure({ nested: true }),
      Table.configure({
        resizable: true,
        HTMLAttributes: {
          class: "md-table",
        },
      }),
      TableRow,
      TableHeader,
      TableCell,
      Placeholder.configure({ placeholder: () => tRef.current("editor.placeholder") }),
      Markdown.configure({
        transformPastedText: true,
        transformCopiedText: true,
      }),
      WikiLink.configure({
        suggestion: createWikiLinkSuggestion(vaultPath),
      }),
      TagHighlight,
      ChemDrawCommand,
      ...(enableScientific
        ? [
            InlineMathWithMarkdown.configure({
              katexOptions: sharedKatexOptions,
              onClick: (node: PmNode, pos: number) => onMathClick(node, pos, false),
            }),
            BlockMathWithMarkdown.configure({
              katexOptions: sharedKatexOptions,
              onClick: (node: PmNode, pos: number) => onMathClick(node, pos, true),
            }),
            DatabaseBlock,
            ...(activeDiscipline === "chemistry" ? [StoichiometryBlock] : []),
          ]
        : []),
    ] as Extension[],
    [vaultPath, enableScientific, activeDiscipline, tRef, onMathClick],
  );
}
