import { useState, useEffect, useCallback, useTransition } from "react";
import { invoke } from "@tauri-apps/api/core";
import type { NoteInfo } from "../types";
import type { TagTreeNode } from "../components/sidebar/TagTree";

interface UseSidebarTagsOptions {
  vaultPath: string;
  notes: NoteInfo[];
  tab: "files" | "tags";
}

export function useSidebarTags({ vaultPath, notes, tab }: UseSidebarTagsOptions) {
  const [tagTree, setTagTree] = useState<TagTreeNode[]>([]);
  const [selectedTag, setSelectedTag] = useState<string | null>(null);
  const [tagNotes, setTagNotes] = useState<NoteInfo[]>([]);
  const [tagNotesPending, startTagNotesTransition] = useTransition();

  /** 递归统计树中有 count > 0 的叶节点总数 */
  const countTags = (nodes: TagTreeNode[]): number =>
    nodes.reduce((sum, n) => sum + (n.count > 0 ? 1 : 0) + countTags(n.children), 0);

  const refreshTags = useCallback(async () => {
    try {
      // 直接从 Rust 获取预构建的树形结构，省去 JS 端 buildTagTree 计算
      const tree = await invoke<TagTreeNode[]>("get_tag_tree");
      setTagTree(tree);
    } catch (e) {
      console.error("加载标签树失败:", e);
    }
  }, []);

  useEffect(() => {
    if (!vaultPath || tab !== "tags") return;
    void refreshTags();
  }, [notes, vaultPath, tab, refreshTags]);

  const handleSelectTag = useCallback(async (tag: string) => {
    let nextTag: string | null = null;
    setSelectedTag(prev => {
      nextTag = prev === tag ? null : tag;
      return nextTag;
    });

    if (!nextTag) {
      setTagNotes([]);
      return;
    }

    startTagNotesTransition(async () => {
      try {
        const result = await invoke<NoteInfo[]>("get_notes_by_tag", { tag: nextTag });
        setTagNotes(result);
      } catch (e) {
        console.error("按标签查询笔记失败:", e);
        setTagNotes([]);
      }
    });
  }, []);

  return {
    tags: { length: countTags(tagTree) },
    tagTree,
    selectedTag,
    tagNotes,
    tagNotesPending,
    handleSelectTag,
  };
}
