import { Mark } from "@tiptap/core";
import { Plugin, PluginKey } from "@tiptap/pm/state";
import { Decoration, DecorationSet } from "@tiptap/pm/view";

/**
 * Tag 行内标签高亮扩展
 *
 * 使用 ProseMirror Decoration 将正文中的 #标签 渲染为带有特殊样式的行内元素，
 * 不修改文档结构，纯视觉增强。
 */

const TAG_REGEX = /(?:^|\s)(#[^\s#]+)/g;

const tagPluginKey = new PluginKey("tagHighlight");

export const TagHighlight = Mark.create({
  name: "tagHighlight",

  addProseMirrorPlugins() {
    return [
      new Plugin({
        key: tagPluginKey,
        state: {
          init(_, { doc }) {
            return buildDecorations(doc);
          },
          apply(tr, oldSet) {
            if (tr.docChanged) {
              return buildDecorations(tr.doc);
            }
            return oldSet;
          },
        },
        props: {
          decorations(state) {
            return this.getState(state);
          },
        },
      }),
    ];
  },
});

function buildDecorations(doc: any): DecorationSet {
  const decorations: Decoration[] = [];

  doc.descendants((node: any, pos: number) => {
    if (!node.isText) return;
    const text = node.text || "";
    let match: RegExpExecArray | null;
    TAG_REGEX.lastIndex = 0;

    while ((match = TAG_REGEX.exec(text)) !== null) {
      const fullMatch = match[0];
      const tag = match[1];
      // Calculate the start position of the #tag within the full match
      const tagStart = pos + match.index + (fullMatch.length - tag.length);
      const tagEnd = tagStart + tag.length;

      decorations.push(
        Decoration.inline(tagStart, tagEnd, {
          class: "inline-tag",
        })
      );
    }
  });

  return DecorationSet.create(doc, decorations);
}

export default TagHighlight;
