import { useCallback, useEffect, useRef } from "react";
import { invoke } from "@tauri-apps/api/core";

interface SaveRequest {
  vaultPath: string;
  filePath: string;
  content: string;
  fingerprint: string;
}

interface UseNotePersistenceOptions {
  onError: (message: string) => void;
}

function hashContent(content: string): string {
  let hash = 2166136261;
  for (let index = 0; index < content.length; index += 1) {
    hash ^= content.charCodeAt(index);
    hash = Math.imul(hash, 16777619);
  }
  return `${content.length}:${hash >>> 0}`;
}

export function useNotePersistence({ onError }: UseNotePersistenceOptions) {
  // 按文件维度排队：每个文件最多保留一个待保存请求
  const pendingByFileRef = useRef<Map<string, SaveRequest>>(new Map());
  const processingRef = useRef(false);
  const waitersRef = useRef<Array<() => void>>([]);
  const lastSavedByFileRef = useRef(new Map<string, string>());

  const resolveWaiters = useCallback(() => {
    const waiters = waitersRef.current.splice(0);
    for (const resolve of waiters) {
      resolve();
    }
  }, []);

  const processQueue = useCallback(async () => {
    if (processingRef.current) {
      return;
    }

    processingRef.current = true;

    try {
      // 循环直到队列清空：每轮取出所有待保存文件，逐个落盘
      while (pendingByFileRef.current.size > 0) {
        // 取出当前快照并清空队列（处理期间新入队的会进入下一轮）
        const batch = new Map(pendingByFileRef.current);
        pendingByFileRef.current.clear();

        for (const [, request] of batch) {
          const lastSaved = lastSavedByFileRef.current.get(request.filePath);
          if (lastSaved === request.fingerprint) {
            continue;
          }

          try {
            await invoke("write_note", {
              vaultPath: request.vaultPath,
              filePath: request.filePath,
              content: request.content,
            });
            lastSavedByFileRef.current.set(request.filePath, request.fingerprint);
          } catch (error) {
            onError(`保存失败 [${request.filePath}]: ${error instanceof Error ? error.message : String(error)}`);
          }
        }
      }
    } finally {
      processingRef.current = false;
      resolveWaiters();
    }
  }, [onError, resolveWaiters]);

  const enqueueSave = useCallback((vaultPath: string, filePath: string, content: string) => {
    if (!vaultPath || !filePath) {
      return;
    }

    const fingerprint = hashContent(content);
    const lastSaved = lastSavedByFileRef.current.get(filePath);
    if (lastSaved === fingerprint) {
      return;
    }

    // 同一文件重复入队时，新请求自动覆盖旧请求（只落盘最后版本）
    pendingByFileRef.current.set(filePath, {
      vaultPath,
      filePath,
      content,
      fingerprint,
    });
    void processQueue();
  }, [processQueue]);

  const flushPendingSave = useCallback(async () => {
    void processQueue();

    if (!processingRef.current && pendingByFileRef.current.size === 0) {
      return;
    }

    await new Promise<void>(resolve => {
      waitersRef.current.push(resolve);
    });

    // 如果在等待期间又有新请求入队，递归等待
    if (pendingByFileRef.current.size > 0 || processingRef.current) {
      await flushPendingSave();
    }
  }, [processQueue]);

  useEffect(() => {
    const handleVisibilityChange = () => {
      if (document.visibilityState === "hidden") {
        void flushPendingSave();
      }
    };

    const handleBeforeUnload = () => {
      void flushPendingSave();
    };

    document.addEventListener("visibilitychange", handleVisibilityChange);
    window.addEventListener("beforeunload", handleBeforeUnload);

    return () => {
      document.removeEventListener("visibilitychange", handleVisibilityChange);
      window.removeEventListener("beforeunload", handleBeforeUnload);
      void flushPendingSave();
    };
  }, [flushPendingSave]);

  return {
    enqueueSave,
    flushPendingSave,
  };
}
