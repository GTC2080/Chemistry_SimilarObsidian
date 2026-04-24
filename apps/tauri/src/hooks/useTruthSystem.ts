/**
 * TRUTH_SYSTEM — 从学习时长推导经验等级
 *
 * 监听 study-tick 事件刷新状态（替代轮询，减少 IPC 调用量）。
 */

import { useState, useEffect, useCallback } from "react";
import { invoke } from "@tauri-apps/api/core";
import type { TruthState } from "../models/truth_system";
import { createDefaultState } from "../models/truth_system";

interface UseTruthSystemOptions {
  /** vault 是否已加载 */
  active: boolean;
}

export function useTruthSystem({ active }: UseTruthSystemOptions) {
  const [state, setState] = useState<TruthState>(createDefaultState);

  const refresh = useCallback(async () => {
    try {
      const result = await invoke<TruthState>("truth_state_from_study");
      setState(result);
    } catch {
      // 查询失败时保持当前状态，不影响 UI
    }
  }, []);

  useEffect(() => {
    if (!active) return;
    // 初始加载
    void refresh();
    // 监听 study tracker 的 tick 事件，替代 30s 轮询
    const onTick = () => void refresh();
    window.addEventListener("study-tick", onTick);
    return () => window.removeEventListener("study-tick", onTick);
  }, [active, refresh]);

  return { truthState: state };
}
