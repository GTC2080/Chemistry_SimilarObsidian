import { useState, useCallback, useRef, useEffect } from "react";

interface UseResizableOptions {
  /** 初始宽度 (px) */
  initialWidth: number;
  /** 最小宽度 (px) */
  minWidth: number;
  /** 最大宽度 (px) */
  maxWidth: number;
  /** 拖拽方向："left" 表示手柄在面板右侧（左侧面板），"right" 表示手柄在面板左侧（右侧面板） */
  side: "left" | "right";
  /** 可选：拖拽时写入此 CSS 变量名（如 "--sidebar-width"），避免 React 重渲染 */
  cssVar?: string;
}

export function useResizable({ initialWidth, minWidth, maxWidth, side, cssVar }: UseResizableOptions) {
  const [width, setWidth] = useState(initialWidth);
  const isDragging = useRef(false);
  const startX = useRef(0);
  const startWidth = useRef(0);
  const rafRef = useRef(0);

  const handleMouseDown = useCallback((e: React.MouseEvent) => {
    e.preventDefault();
    isDragging.current = true;
    startX.current = e.clientX;
    startWidth.current = width;
    document.body.style.cursor = "col-resize";
    document.body.style.userSelect = "none";
  }, [width]);

  useEffect(() => {
    function onMouseMove(e: MouseEvent) {
      if (!isDragging.current) return;

      // 使用 rAF 节流：每帧只更新一次
      if (rafRef.current) return;
      rafRef.current = requestAnimationFrame(() => {
        rafRef.current = 0;
        const delta = e.clientX - startX.current;
        const newWidth = side === "left"
          ? startWidth.current + delta
          : startWidth.current - delta;
        const clamped = Math.min(maxWidth, Math.max(minWidth, newWidth));

        if (cssVar) {
          // 拖拽期间只更新 CSS 变量，不触发 React 重渲染
          document.documentElement.style.setProperty(cssVar, `${clamped}px`);
        } else {
          setWidth(clamped);
        }
      });
    }

    function onMouseUp() {
      if (!isDragging.current) return;
      isDragging.current = false;
      document.body.style.cursor = "";
      document.body.style.userSelect = "";

      if (rafRef.current) {
        cancelAnimationFrame(rafRef.current);
        rafRef.current = 0;
      }

      if (cssVar) {
        // 拖拽结束：从 CSS 变量读取最终值，回写 React state（一次渲染）
        const raw = document.documentElement.style.getPropertyValue(cssVar);
        const finalWidth = parseFloat(raw);
        if (!Number.isNaN(finalWidth)) {
          setWidth(Math.min(maxWidth, Math.max(minWidth, finalWidth)));
        }
      }
    }

    window.addEventListener("mousemove", onMouseMove);
    window.addEventListener("mouseup", onMouseUp);
    return () => {
      window.removeEventListener("mousemove", onMouseMove);
      window.removeEventListener("mouseup", onMouseUp);
      if (rafRef.current) {
        cancelAnimationFrame(rafRef.current);
      }
    };
  }, [minWidth, maxWidth, side, cssVar]);

  return { width, handleMouseDown };
}
