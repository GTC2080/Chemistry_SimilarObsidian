import { Node, mergeAttributes } from "@tiptap/core";
import { ReactNodeViewRenderer } from "@tiptap/react";
import DatabaseGrid from "../../components/editor/DatabaseGrid";
import {
  createDefaultDatabasePayload,
  normalizeDatabasePayload,
  parseDatabaseCodeBlock,
  serializeDatabaseCodeBlock,
  type DatabasePayload,
} from "../schema/database";

function escapeHtml(input: string): string {
  return input
    .replace(/&/g, "&amp;")
    .replace(/</g, "&lt;")
    .replace(/>/g, "&gt;")
    .replace(/"/g, "&quot;");
}

declare module "@tiptap/core" {
  interface Commands<ReturnType> {
    databaseBlock: {
      insertDatabaseBlock: () => ReturnType;
    };
  }
}

export const DatabaseBlock = Node.create({
  name: "databaseBlock",
  group: "block",
  content: "inline*",
  atom: true,
  isolating: true,
  draggable: false,

  addAttributes() {
    const defaultPayload = createDefaultDatabasePayload();
    return {
      columns: {
        default: defaultPayload.columns,
      },
      rows: {
        default: defaultPayload.rows,
      },
    };
  },

  parseHTML() {
    return [{
      tag: 'div[data-type="database-block"]',
      getAttrs: element => {
        const raw = (element as HTMLElement).getAttribute("data-database");
        if (!raw) return createDefaultDatabasePayload();
        try {
          const decoded = decodeURIComponent(raw);
          const fromFence = parseDatabaseCodeBlock(decoded);
          if (fromFence) return fromFence;
          const parsed = JSON.parse(decoded) as DatabasePayload;
          return normalizeDatabasePayload(parsed);
        } catch {
          return createDefaultDatabasePayload();
        }
      },
    }];
  },

  renderHTML({ HTMLAttributes, node }) {
    const payload = normalizeDatabasePayload(node.attrs);
    const fenced = serializeDatabaseCodeBlock(payload);
    return [
      "div",
      mergeAttributes(HTMLAttributes, {
        "data-type": "database-block",
        "data-database": encodeURIComponent(JSON.stringify(payload)),
      }),
      ["pre", ["code", fenced]],
    ];
  },

  addNodeView() {
    return ReactNodeViewRenderer(DatabaseGrid);
  },

  addCommands() {
    return {
      insertDatabaseBlock:
        () =>
          ({ commands }) =>
            commands.insertContent({
              type: this.name,
              attrs: createDefaultDatabasePayload(),
            }),
    };
  },

  addKeyboardShortcuts() {
    return {
      "Mod-Shift-d": () => this.editor.commands.insertDatabaseBlock(),
    };
  },

  addStorage() {
    return {
      markdown: {
        serialize(state: any, node: any) {
          const payload = normalizeDatabasePayload(node.attrs);
          state.write(serializeDatabaseCodeBlock(payload));
          state.closeBlock(node);
        },
        parse: {
          setup(markdownit: any) {
            markdownit.block.ruler.before(
              "fence",
              "database_block",
              (state: any, startLine: number, endLine: number, silent: boolean) => {
                const startPos = state.bMarks[startLine] + state.tShift[startLine];
                const maxPos = state.eMarks[startLine];
                const line = state.src.slice(startPos, maxPos).trim();
                if (line !== "```database") return false;
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

                let payload = createDefaultDatabasePayload();
                if (rawJson) {
                  try {
                    payload = normalizeDatabasePayload(JSON.parse(rawJson));
                  } catch {
                    payload = createDefaultDatabasePayload();
                  }
                }

                const token = state.push("database_block", "div", 0);
                token.block = true;
                token.map = [startLine, endFenceLine + 1];
                token.meta = { payload };
                state.line = endFenceLine + 1;
                return true;
              }
            );

            markdownit.renderer.rules.database_block = (tokens: any[], idx: number) => {
              const payload = normalizeDatabasePayload(tokens[idx].meta?.payload);
              const encoded = encodeURIComponent(JSON.stringify(payload));
              return `<div data-type="database-block" data-database="${encoded}">${escapeHtml(
                serializeDatabaseCodeBlock(payload)
              )}</div>\n`;
            };
          },
        },
      },
    };
  },
});
