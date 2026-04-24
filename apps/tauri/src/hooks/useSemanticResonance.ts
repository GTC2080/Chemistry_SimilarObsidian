import { useEffect, useRef, useState } from "react";
import { invoke } from "@tauri-apps/api/core";
import type { NoteInfo } from "../types";

export const MIN_CONTEXT_CHARS = 24;
const CACHE_LIMIT = 40;

export function getAdaptiveDebounceMs(contentLength: number) {
  if (contentLength < 400) return 900;
  if (contentLength < 1400) return 1500;
  if (contentLength < 3200) return 2200;
  return 2800;
}

/** 轻量缓存 key：用长度 + 尾部 64 字符做指纹，避免 JS 端哈希计算 */
function cheapCacheKey(content: string, noteId: string): string {
  const tail = content.length <= 64 ? content : content.slice(-64);
  return `${noteId}:${content.length}:${tail}`;
}

function cacheResult(cache: Map<string, NoteInfo[]>, key: string, value: NoteInfo[]) {
  if (cache.has(key)) {
    cache.delete(key);
  }
  cache.set(key, value);
  while (cache.size > CACHE_LIMIT) {
    const oldest = cache.keys().next().value;
    if (!oldest) break;
    cache.delete(oldest);
  }
}

/**
 * 语义共鸣 hook：传递原始内容到 Rust 后端，
 * 由 Rust 完成语义上下文提取 + embedding 搜索。
 */
export function useSemanticResonance(
  content: string,
  currentNoteId: string | null,
  enabled = true,
) {
  const [relatedNotes, setRelatedNotes] = useState<NoteInfo[]>([]);
  const [loading, setLoading] = useState(false);
  const timerRef = useRef<ReturnType<typeof setTimeout> | null>(null);
  const requestIdRef = useRef(0);
  const cacheRef = useRef(new Map<string, NoteInfo[]>());

  // Track previous content length to detect significant change
  const prevLenRef = useRef(0);

  useEffect(() => {
    if (timerRef.current) {
      clearTimeout(timerRef.current);
    }

    const trimmed = content.trim();
    if (!enabled || !currentNoteId || trimmed.length < MIN_CONTEXT_CHARS) {
      setRelatedNotes([]);
      setLoading(false);
      prevLenRef.current = 0;
      return;
    }

    const cacheKey = cheapCacheKey(trimmed, currentNoteId);
    const cached = cacheRef.current.get(cacheKey);
    if (cached) {
      setRelatedNotes(cached);
      setLoading(false);
      prevLenRef.current = trimmed.length;
      return;
    }

    // Skip if change is too small (< 20 chars difference) — avoids
    // triggering expensive embedding search for minor edits.
    const lenDelta = Math.abs(trimmed.length - prevLenRef.current);
    if (prevLenRef.current > 0 && lenDelta < 20) {
      return;
    }

    const debounceMs = getAdaptiveDebounceMs(trimmed.length);
    timerRef.current = setTimeout(async () => {
      prevLenRef.current = trimmed.length;
      const thisRequestId = ++requestIdRef.current;
      setLoading(true);

      try {
        // 直接传原始内容到 Rust，由 Rust 端做 context 提取 + embedding 搜索
        const results = await invoke<NoteInfo[]>("get_related_notes_raw", {
          rawContent: trimmed,
          currentNoteId,
          limit: 5,
        });

        if (thisRequestId === requestIdRef.current) {
          cacheResult(cacheRef.current, cacheKey, results);
          setRelatedNotes(results);
        }
      } catch {
        if (thisRequestId === requestIdRef.current) {
          setRelatedNotes([]);
        }
      } finally {
        if (thisRequestId === requestIdRef.current) {
          setLoading(false);
        }
      }
    }, debounceMs);

    return () => {
      if (timerRef.current) {
        clearTimeout(timerRef.current);
      }
    };
  }, [content, currentNoteId, enabled]);

  return { relatedNotes, loading };
}
