import { createContext, useContext, useRef, useCallback } from "react";
import { invoke } from "@tauri-apps/api/core";
import type { ReactNode } from "react";

const CACHE_LIMIT = 20;

interface CacheEntry {
  content: string;
  updatedAt: number;
}

interface NoteContentCacheContextValue {
  /** 读取笔记内容，带 LRU 缓存（基于 updatedAt 判断是否过期） */
  readNote: (filePath: string, updatedAt: number) => Promise<string>;
  /** 写入后手动失效缓存 */
  invalidate: (filePath: string) => void;
}

const NoteContentCacheContext = createContext<NoteContentCacheContextValue | null>(null);

export function NoteContentCacheProvider({ children }: { children: ReactNode }) {
  const cacheRef = useRef(new Map<string, CacheEntry>());

  const readNote = useCallback(async (filePath: string, updatedAt: number): Promise<string> => {
    const cached = cacheRef.current.get(filePath);
    if (cached && cached.updatedAt >= updatedAt) {
      // LRU: 移到末尾
      cacheRef.current.delete(filePath);
      cacheRef.current.set(filePath, cached);
      return cached.content;
    }

    const content = await invoke<string>("read_note", { filePath });
    cacheRef.current.set(filePath, { content, updatedAt });

    // 淘汰最旧条目
    while (cacheRef.current.size > CACHE_LIMIT) {
      const oldest = cacheRef.current.keys().next().value;
      if (oldest) cacheRef.current.delete(oldest);
    }

    return content;
  }, []);

  const invalidate = useCallback((filePath: string) => {
    cacheRef.current.delete(filePath);
  }, []);

  return (
    <NoteContentCacheContext.Provider value={{ readNote, invalidate }}>
      {children}
    </NoteContentCacheContext.Provider>
  );
}

export function useNoteContentCache() {
  const ctx = useContext(NoteContentCacheContext);
  if (!ctx) throw new Error("useNoteContentCache must be used within NoteContentCacheProvider");
  return ctx;
}
