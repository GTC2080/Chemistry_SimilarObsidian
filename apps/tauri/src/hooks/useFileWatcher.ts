import { useEffect, useRef, useCallback, type Dispatch, type SetStateAction } from "react";
import { invoke } from "@tauri-apps/api/core";
import { listen, type UnlistenFn } from "@tauri-apps/api/event";
import type { NoteInfo } from "../types";

interface FsChangeEvent {
  changed: string[];
  removed: string[];
}

interface UseFileWatcherOptions {
  vaultPath: string;
  ignoredFolders: string;
  setNotes: Dispatch<SetStateAction<NoteInfo[]>>;
}

/**
 * 文件系统增量监听 hook。
 *
 * 当 vault 打开时自动启动 Rust 端的文件监听器，
 * 收到 `vault:fs-change` 事件后增量更新 notes 列表：
 * - removed: 从列表中移除对应文件，并从 DB 中清理
 * - changed: 只扫描变更文件的元数据并 merge 进现有列表
 *
 * vault 关闭或切换时自动停止旧的监听器。
 */
export function useFileWatcher({
  vaultPath,
  ignoredFolders,
  setNotes,
}: UseFileWatcherOptions) {
  const vaultPathRef = useRef(vaultPath);
  vaultPathRef.current = vaultPath;

  const handleFsChange = useCallback(
    async (event: { payload: FsChangeEvent }) => {
      const { changed, removed } = event.payload;
      const currentVault = vaultPathRef.current;
      if (!currentVault) return;

      // 1. 处理删除：从前端列表中移除 + 从 DB 中清理
      if (removed.length > 0) {
        const removedSet = new Set(
          removed.map((r) => r.replace(/\\/g, "/"))
        );
        setNotes((prev) =>
          prev.filter((n) => !removedSet.has(n.id.replace(/\\/g, "/")))
        );

        // 后台清理 DB 中已删除文件的索引
        invoke("remove_deleted_entries", { paths: removed }).catch(
          (err: unknown) => {
            console.warn("[remove_deleted_entries]", err);
          }
        );
      }

      // 2. 处理变更：只扫描变更文件的元数据，merge 进现有列表
      if (changed.length > 0) {
        try {
          const freshEntries = await invoke<NoteInfo[]>(
            "scan_changed_entries",
            {
              vaultPath: currentVault,
              paths: changed,
            }
          );

          if (freshEntries.length > 0) {
            setNotes((prev) => {
              const map = new Map(prev.map((n) => [n.id, n]));
              for (const entry of freshEntries) {
                map.set(entry.id, entry);
              }
              // 保持按 updated_at 降序
              return Array.from(map.values()).sort(
                (a, b) => b.updated_at - a.updated_at
              );
            });
          }

          // 后台增量索引（只处理变更文件的内容）
          invoke("index_changed_entries", {
            vaultPath: currentVault,
            paths: changed,
          }).catch((err: unknown) => {
            console.warn("[index_changed_entries]", err);
          });
        } catch (err) {
          console.warn("[useFileWatcher] scan_changed_entries failed:", err);
        }
      }
    },
    [setNotes]
  );

  useEffect(() => {
    if (!vaultPath) return;

    let unlisten: UnlistenFn | null = null;
    let stopped = false;

    const setup = async () => {
      try {
        await invoke("start_watcher", {
          vaultPath,
          ignoredFolders: ignoredFolders || "",
        });
      } catch (err) {
        console.warn("[start_watcher]", err);
        return;
      }

      if (stopped) {
        invoke("stop_watcher").catch(() => {});
        return;
      }

      unlisten = await listen<FsChangeEvent>(
        "vault:fs-change",
        handleFsChange
      );
    };

    setup();

    return () => {
      stopped = true;
      unlisten?.();
      invoke("stop_watcher").catch(() => {});
    };
  }, [vaultPath, ignoredFolders, handleFsChange]);
}
