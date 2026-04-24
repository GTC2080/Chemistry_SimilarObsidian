import { createRoot, type Root } from "react-dom/client";
import type { SuggestionOptions, SuggestionProps } from "@tiptap/suggestion";
import { invoke } from "@tauri-apps/api/core";
import {
  SuggestionMenu,
  type SuggestionMenuRef,
} from "../components/search";
import type { NoteInfo } from "../types";

/**
 * WikiLink Suggestion 配置工厂函数。
 *
 * 返回一个 Partial<SuggestionOptions> 对象，传给 WikiLink 扩展的 suggestion 选项。
 * 它负责：
 * 1. items：用户每次按键时，调用 Rust 后端的 search_notes 命令查询匹配笔记
 * 2. render：管理 SuggestionMenu React 组件的生命周期（创建、更新、销毁）
 * 3. command：用户选中某项后，将 WikiLink 节点插入编辑器
 *
 * @param vaultPath 当前知识库路径，传给后端搜索命令
 */
export function createWikiLinkSuggestion(
  vaultPath: string
): Partial<SuggestionOptions<NoteInfo>> {
  return {
    /**
     * items 查询函数：Suggestion 插件在每次按键时调用。
     * query 是 [[ 之后用户输入的文本（如 "日记" → query="日记"）。
     */
    items: async ({ query }) => {
      if (!vaultPath) return [];
      try {
        return await invoke<NoteInfo[]>("search_notes", {
          query,
        });
      } catch (e) {
        console.error("搜索笔记失败:", e);
        return [];
      }
    },

    /**
     * command：用户选中某项后执行。
     * 删除 [[ 触发文本，插入 WikiLink 节点。
     */
    command: ({ editor, range, props: item }) => {
      editor
        .chain()
        .focus()
        // deleteRange 删除 [[ 和用户输入的查询文本
        .deleteRange(range)
        // 插入 WikiLink 行内节点
        .insertContent({
          type: "wikiLink",
          attrs: { title: item.name },
        })
        .run();
    },

    /**
     * render：管理悬浮菜单 React 组件的完整生命周期。
     *
     * TipTap Suggestion 要求 render() 返回一个对象，包含：
     * - onStart：首次触发时创建 DOM 容器和 React 组件
     * - onUpdate：查询文本变化时更新组件 props
     * - onKeyDown：转发键盘事件给组件
     * - onExit：关闭菜单时清理 DOM 和 React 根节点
     *
     * 这里使用原生 DOM 操作创建容器，再用 React createRoot 挂载组件，
     * 是 TipTap Suggestion + React 的标准集成模式。
     */
    render: () => {
      let container: HTMLDivElement | null = null;
      let root: Root | null = null;
      let menuRef: SuggestionMenuRef | null = null;

      return {
        onStart(props: SuggestionProps<NoteInfo>) {
          // 创建 DOM 容器，绝对定位在光标下方
          container = document.createElement("div");
          container.style.position = "absolute";
          container.style.zIndex = "50";
          document.body.appendChild(container);

          // 定位到光标位置
          if (props.clientRect) {
            const rect = props.clientRect();
            if (rect) {
              container.style.left = `${rect.left}px`;
              container.style.top = `${rect.bottom + 4}px`;
            }
          }

          root = createRoot(container);
          root.render(
            <SuggestionMenu
              ref={(ref) => { menuRef = ref; }}
              items={props.items as NoteInfo[]}
              command={(item) => props.command(item)}
            />
          );
        },

        onUpdate(props: SuggestionProps<NoteInfo>) {
          // 更新菜单位置和内容
          if (container && props.clientRect) {
            const rect = props.clientRect();
            if (rect) {
              container.style.left = `${rect.left}px`;
              container.style.top = `${rect.bottom + 4}px`;
            }
          }

          root?.render(
            <SuggestionMenu
              ref={(ref) => { menuRef = ref; }}
              items={props.items as NoteInfo[]}
              command={(item) => props.command(item)}
            />
          );
        },

        onKeyDown(props: { event: KeyboardEvent }) {
          // Escape 键关闭菜单
          if (props.event.key === "Escape") return false;
          // 转发给 SuggestionMenu 处理上下键和 Enter
          return menuRef?.onKeyDown(props) ?? false;
        },

        onExit() {
          // 清理：卸载 React 组件，移除 DOM 容器
          // 使用 setTimeout 确保 React 渲染周期完成后再卸载，防止警告
          const currentRoot = root;
          const currentContainer = container;
          setTimeout(() => {
            currentRoot?.unmount();
            currentContainer?.remove();
          }, 0);
          root = null;
          container = null;
          menuRef = null;
        },
      };
    },
  };
}
