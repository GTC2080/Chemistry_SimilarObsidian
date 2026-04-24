import { Node, mergeAttributes } from "@tiptap/core";
import { Node as ProseMirrorNode } from "@tiptap/pm/model";
import Suggestion, { type SuggestionOptions } from "@tiptap/suggestion";

/**
 * WikiLink 自定义节点扩展
 *
 * 在 ProseMirror 文档树中表示为一个行内节点（inline node），
 * 携带 `title` 属性存储链接目标笔记的名称。
 *
 * 渲染为：<span data-type="wiki-link" data-title="笔记名" class="wiki-link">笔记名</span>
 *
 * Markdown 序列化/反序列化：
 * - 保存时：WikiLink 节点 → [[笔记名]]
 * - 加载时：[[笔记名]] → WikiLink 节点
 * 这两个方向的转换通过 tiptap-markdown 的 addStorage 钩子注册。
 */

export interface WikiLinkOptions {
  /** Suggestion 配置，由外部传入（包含 items 查询函数和 render 渲染器） */
  suggestion: Partial<SuggestionOptions>;
}

export const WikiLink = Node.create<WikiLinkOptions>({
  name: "wikiLink",

  // 行内节点，可以出现在段落文本中
  group: "inline",
  inline: true,

  // 原子节点：光标不会进入内部，整体选中/删除
  atom: true,

  // 节点属性：存储链接目标的笔记标题
  addAttributes() {
    return {
      title: {
        default: null,
        // 从 DOM 解析时，从 data-title 属性读取
        parseHTML: (element) => element.getAttribute("data-title"),
        // 渲染到 DOM 时，写入 data-title 属性
        renderHTML: (attributes) => ({ "data-title": attributes.title }),
      },
    };
  },

  // 从 HTML 解析：匹配 span[data-type="wiki-link"]
  parseHTML() {
    return [{ tag: 'span[data-type="wiki-link"]' }];
  },

  // 渲染为 HTML：生成带有特定 data 属性和 CSS 类的 span
  renderHTML({ node, HTMLAttributes }) {
    return [
      "span",
      mergeAttributes(HTMLAttributes, {
        "data-type": "wiki-link",
        class: "wiki-link",
      }),
      `[[${node.attrs.title}]]`,
    ];
  },

  // 纯文本输出（用于复制粘贴等场景）
  renderText({ node }) {
    return `[[${node.attrs.title}]]`;
  },

  // 注册 Suggestion 插件：监听 [[ 触发字符
  addProseMirrorPlugins() {
    return [
      Suggestion({
        editor: this.editor,
        // 触发字符：两个连续的左方括号
        char: "[[",
        // 允许空查询（刚输入 [[ 时就弹出菜单）
        allowSpaces: true,
        // 合并外部传入的 suggestion 配置（items、render 等）
        ...this.options.suggestion,
      }),
    ];
  },

  /**
   * 为 tiptap-markdown 注册自定义的序列化/反序列化规则。
   *
   * addStorage 返回的对象会挂载到 editor.storage.wikiLink 上，
   * tiptap-markdown 会检查每个扩展的 storage.markdown 字段，
   * 如果存在 serialize 和 parse 配置，就会用它们处理该节点类型。
   */
  addStorage() {
    return {
      markdown: {
        // 序列化：ProseMirror WikiLink 节点 → Markdown 文本 [[title]]
        serialize(state: any, node: ProseMirrorNode) {
          state.write(`[[${node.attrs.title}]]`);
        },
        // 反序列化：告诉 tiptap-markdown 如何从 Markdown 文本中识别 [[...]]
        parse: {
          // 使用 markdown-it 的 inline rule 来匹配 [[...]] 语法
          setup(markdownit: any) {
            // 注册一个自定义的 inline rule
            markdownit.inline.ruler.push("wiki_link", (state: any, silent: boolean) => {
              const src = state.src;
              const pos = state.pos;
              const max = state.posMax;

              // 检查是否以 [[ 开头
              if (pos + 3 >= max) return false;
              if (src.charCodeAt(pos) !== 0x5B || src.charCodeAt(pos + 1) !== 0x5B) {
                return false;
              }

              // 查找匹配的 ]]
              const start = pos + 2;
              let end = src.indexOf("]]", start);
              if (end === -1) return false;

              // silent 模式下只检测不生成 token
              if (silent) return true;

              const title = src.slice(start, end);

              // 生成 token
              const token = state.push("wiki_link", "span", 0);
              token.attrs = { title };
              token.content = title;

              // 移动解析位置到 ]] 之后
              state.pos = end + 2;
              return true;
            });

            // 注册 token 渲染规则，将 wiki_link token 映射为 wikiLink 节点
            markdownit.renderer.rules.wiki_link = (tokens: any[], idx: number) => {
              const title = tokens[idx].attrs?.title || tokens[idx].content;
              return `<span data-type="wiki-link" data-title="${title}">[[${title}]]</span>`;
            };
          },
        },
      },
    };
  },
});
