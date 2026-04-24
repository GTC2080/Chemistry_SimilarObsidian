import { useRef, useCallback, useEffect } from "react";

/**
 * 防抖 Hook：返回一个防抖版本的回调函数。
 * 在最后一次调用后等待 delay 毫秒才真正执行，
 * 组件卸载时自动清理定时器，防止内存泄漏。
 */
export function useDebounce<T extends (...args: never[]) => void>(
  fn: T,
  delay: number
): T {
  const timerRef = useRef<ReturnType<typeof setTimeout> | null>(null);
  const fnRef = useRef(fn);
  fnRef.current = fn;

  // 组件卸载时清理
  useEffect(() => {
    return () => {
      if (timerRef.current) clearTimeout(timerRef.current);
    };
  }, []);

  return useCallback(
    (...args: Parameters<T>) => {
      if (timerRef.current) clearTimeout(timerRef.current);
      timerRef.current = setTimeout(() => fnRef.current(...args), delay);
    },
    [delay]
  ) as T;
}
