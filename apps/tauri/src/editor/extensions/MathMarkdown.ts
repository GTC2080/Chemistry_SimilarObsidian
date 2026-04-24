import { InlineMath, BlockMath } from "@tiptap/extension-mathematics";
import { InputRule } from "@tiptap/core";
import "katex/contrib/mhchem";

/** Shared KaTeX options: enable mhchem, relaxed parsing */
export const sharedKatexOptions = {
  strict: false,
  trust: true,
  throwOnError: false,
};

/**
 * Helper: escape HTML special chars in LaTeX strings
 */
function escapeHtml(s: string): string {
  return s.replace(/&/g, "&amp;").replace(/</g, "&lt;").replace(/>/g, "&gt;").replace(/"/g, "&quot;");
}

/**
 * Extends InlineMath with:
 * 1. tiptap-markdown serialization ($...$)
 * 2. markdown-it parse rule for $...$
 * 3. Standard LaTeX input rule: typing $...$ creates inline math
 */
export const InlineMathWithMarkdown = InlineMath.extend({
  addStorage() {
    return {
      ...this.parent?.(),
      markdown: {
        serialize(state: any, node: any) {
          state.write("$" + node.attrs.latex + "$");
        },
        parse: {
          setup(markdownit: any) {
            // Register inline rule: $...$ → <span data-type="inline-math" data-latex="...">
            markdownit.inline.ruler.after("escape", "inline_math", (state: any, silent: boolean) => {
              const src = state.src;
              const pos = state.pos;
              const max = state.posMax;

              if (src.charCodeAt(pos) !== 0x24 /* $ */) return false;
              // Must not be $$ (that's block math)
              if (pos + 1 < max && src.charCodeAt(pos + 1) === 0x24) return false;
              // Must not be preceded by $ (avoid matching inside $$)
              if (pos > 0 && src.charCodeAt(pos - 1) === 0x24) return false;

              // Find closing $
              let end = pos + 1;
              while (end < max) {
                if (src.charCodeAt(end) === 0x24 /* $ */) {
                  // Make sure closing $ is not escaped
                  if (end > 0 && src.charCodeAt(end - 1) === 0x5C /* \ */) {
                    end++;
                    continue;
                  }
                  break;
                }
                end++;
              }

              if (end >= max) return false; // No closing $
              const latex = src.slice(pos + 1, end);
              if (!latex.trim()) return false;

              if (silent) return true;

              const token = state.push("inline_math", "", 0);
              token.content = latex;
              state.pos = end + 1;
              return true;
            });

            // Render inline math to HTML
            markdownit.renderer.rules.inline_math = (tokens: any[], idx: number) => {
              const latex = tokens[idx].content;
              return `<span data-type="inline-math" data-latex="${escapeHtml(latex)}"></span>`;
            };
          },
        },
      },
    };
  },

  addInputRules() {
    return [
      // Standard: $...$ (single dollar)
      new InputRule({
        find: /(^|[^$\\])(\$([^$\n]+?)\$)(?!\$)/,
        handler: ({ state, range, match }) => {
          const latex = match[3];
          if (!latex?.trim()) return;
          const { tr } = state;
          const prefixLen = match[1].length;
          tr.replaceWith(
            range.from + prefixLen,
            range.to,
            this.type.create({ latex: latex.trim() }),
          );
        },
      }),
      // Also keep the original $$...$$ pattern for compatibility
      ...this.parent?.() ?? [],
    ];
  },
});

/**
 * Extends BlockMath with:
 * 1. tiptap-markdown serialization ($$...$$)
 * 2. markdown-it parse rule for $$...$$
 * 3. Standard LaTeX input rule: typing $$...$$ on its own line creates block math
 */
export const BlockMathWithMarkdown = BlockMath.extend({
  addStorage() {
    return {
      ...this.parent?.(),
      markdown: {
        serialize(state: any, node: any) {
          state.write("$$\n" + node.attrs.latex + "\n$$");
          state.closeBlock(node);
        },
        parse: {
          setup(markdownit: any) {
            // Register block rule: $$...$$ → <div data-type="block-math" data-latex="...">
            markdownit.block.ruler.before("fence", "block_math", (state: any, startLine: number, endLine: number, silent: boolean) => {
              const startPos = state.bMarks[startLine] + state.tShift[startLine];
              const maxPos = state.eMarks[startLine];
              const lineText = state.src.slice(startPos, maxPos);

              // Line must start with $$
              if (lineText.slice(0, 2) !== "$$") return false;

              // Case 1: Single-line $$...$$
              const inlineContent = lineText.slice(2).trim();
              if (inlineContent.endsWith("$$") && inlineContent.length > 2) {
                const latex = inlineContent.slice(0, -2).trim();
                if (!latex) return false;
                if (silent) return true;

                const token = state.push("block_math", "", 0);
                token.content = latex;
                token.map = [startLine, startLine + 1];
                state.line = startLine + 1;
                return true;
              }

              // Case 2: Multi-line — find closing $$
              let nextLine = startLine + 1;
              let found = false;
              while (nextLine < endLine) {
                const nextStart = state.bMarks[nextLine] + state.tShift[nextLine];
                const nextMax = state.eMarks[nextLine];
                const nextText = state.src.slice(nextStart, nextMax).trim();

                if (nextText === "$$") {
                  found = true;
                  break;
                }
                nextLine++;
              }

              if (!found) return false;
              if (silent) return true;

              // Collect content between the $$ markers
              const contentLines: string[] = [];
              for (let i = startLine + 1; i < nextLine; i++) {
                contentLines.push(state.src.slice(state.bMarks[i] + state.tShift[i], state.eMarks[i]));
              }

              // Also include anything after the opening $$ on the same line
              const firstLineRemainder = lineText.slice(2).trim();
              let latex: string;
              if (firstLineRemainder) {
                contentLines.unshift(firstLineRemainder);
              }
              latex = contentLines.join("\n").trim();

              if (!latex) return false;

              const token = state.push("block_math", "", 0);
              token.content = latex;
              token.map = [startLine, nextLine + 1];
              state.line = nextLine + 1;
              return true;
            });

            // Render block math to HTML
            markdownit.renderer.rules.block_math = (tokens: any[], idx: number) => {
              const latex = tokens[idx].content;
              return `<div data-type="block-math" data-latex="${escapeHtml(latex)}"></div>`;
            };
          },
        },
      },
    };
  },

  addInputRules() {
    return [
      // Standard: $$...$$ at the start of a line
      new InputRule({
        find: /^\$\$([^$]+)\$\$/,
        handler: ({ state, range, match }) => {
          const latex = match[1];
          if (!latex?.trim()) return;
          const { tr } = state;
          tr.replaceWith(range.from, range.to, this.type.create({ latex: latex.trim() }));
        },
      }),
      // Also keep the original $$$...$$$ pattern for compatibility
      ...this.parent?.() ?? [],
    ];
  },
});
