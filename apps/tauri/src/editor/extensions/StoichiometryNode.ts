import { Node, mergeAttributes } from "@tiptap/core";
import { ReactNodeViewRenderer } from "@tiptap/react";
import StoichiometryGrid from "../../components/editor/StoichiometryGrid";
import {
  createDefaultStoichiometryRows,
  normalizeStoichiometryRows,
  parseStoichiometryCodeBlock,
  serializeStoichiometryCodeBlock,
} from "../schema/stoichiometry";

function escapeHtml(input: string): string {
  return input
    .replace(/&/g, "&amp;")
    .replace(/</g, "&lt;")
    .replace(/>/g, "&gt;")
    .replace(/"/g, "&quot;");
}

declare module "@tiptap/core" {
  interface Commands<ReturnType> {
    stoichiometryBlock: {
      insertStoichiometryBlock: () => ReturnType;
    };
  }
}

export const StoichiometryBlock = Node.create({
  name: "stoichiometryBlock",
  group: "block",
  content: "inline*",
  atom: true,
  isolating: true,
  draggable: false,

  addAttributes() {
    return {
      rows: {
        default: createDefaultStoichiometryRows(),
      },
    };
  },

  parseHTML() {
    return [
      {
        tag: 'div[data-type="stoichiometry-block"]',
        getAttrs: element => {
          const raw = (element as HTMLElement).getAttribute("data-stoichiometry");
          if (!raw) return { rows: createDefaultStoichiometryRows() };
          try {
            const decoded = decodeURIComponent(raw);
            const fromFence = parseStoichiometryCodeBlock(decoded);
            if (fromFence) return { rows: fromFence };
            return { rows: normalizeStoichiometryRows(JSON.parse(decoded)) };
          } catch {
            return { rows: createDefaultStoichiometryRows() };
          }
        },
      },
    ];
  },

  renderHTML({ HTMLAttributes, node }) {
    const rows = normalizeStoichiometryRows(node.attrs.rows);
    const fenced = serializeStoichiometryCodeBlock(rows);
    return [
      "div",
      mergeAttributes(HTMLAttributes, {
        "data-type": "stoichiometry-block",
        "data-stoichiometry": encodeURIComponent(JSON.stringify(rows)),
      }),
      ["pre", ["code", fenced]],
    ];
  },

  addNodeView() {
    return ReactNodeViewRenderer(StoichiometryGrid);
  },

  addCommands() {
    return {
      insertStoichiometryBlock:
        () =>
          ({ commands }) =>
            commands.insertContent({
              type: this.name,
              attrs: { rows: createDefaultStoichiometryRows() },
            }),
    };
  },

  addKeyboardShortcuts() {
    return {
      "Mod-Shift-s": () => this.editor.commands.insertStoichiometryBlock(),
    };
  },

  addStorage() {
    return {
      markdown: {
        serialize(state: any, node: any) {
          const rows = normalizeStoichiometryRows(node.attrs.rows);
          state.write(serializeStoichiometryCodeBlock(rows));
          state.closeBlock(node);
        },
        parse: {
          setup(markdownit: any) {
            markdownit.block.ruler.before(
              "fence",
              "stoichiometry_block",
              (state: any, startLine: number, endLine: number, silent: boolean) => {
                const startPos = state.bMarks[startLine] + state.tShift[startLine];
                const maxPos = state.eMarks[startLine];
                const line = state.src.slice(startPos, maxPos).trim();
                if (line !== "```stoichiometry") return false;
                if (silent) return true;

                let nextLine = startLine + 1;
                while (nextLine < endLine) {
                  const closeStart = state.bMarks[nextLine] + state.tShift[nextLine];
                  const closeMax = state.eMarks[nextLine];
                  const closeLine = state.src.slice(closeStart, closeMax).trim();
                  if (closeLine === "```") break;
                  nextLine += 1;
                }

                const endFenceLine = nextLine < endLine ? nextLine : endLine - 1;
                const contentLine = Math.min(startLine + 1, endFenceLine);
                const contentStart = state.bMarks[contentLine] + state.tShift[contentLine];
                const contentEnd = state.bMarks[endFenceLine];
                const rawJson = state.src.slice(contentStart, contentEnd).trim();

                let rows = createDefaultStoichiometryRows();
                if (rawJson) {
                  try {
                    rows = normalizeStoichiometryRows(JSON.parse(rawJson));
                  } catch {
                    rows = createDefaultStoichiometryRows();
                  }
                }

                const token = state.push("stoichiometry_block", "div", 0);
                token.block = true;
                token.map = [startLine, endFenceLine + 1];
                token.meta = { rows };
                state.line = endFenceLine + 1;
                return true;
              }
            );

            markdownit.renderer.rules.stoichiometry_block = (tokens: any[], idx: number) => {
              const rows = normalizeStoichiometryRows(tokens[idx].meta?.rows);
              const encoded = encodeURIComponent(JSON.stringify(rows));
              return `<div data-type="stoichiometry-block" data-stoichiometry="${encoded}">${escapeHtml(
                serializeStoichiometryCodeBlock(rows)
              )}</div>\n`;
            };
          },
        },
      },
    };
  },
});
