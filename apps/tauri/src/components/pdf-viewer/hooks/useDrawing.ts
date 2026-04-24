/**
 * 绘图模式：画笔状态、笔画提交、Rust 平滑
 */

import { useCallback, useRef, useState } from "react";
import { invoke } from "@tauri-apps/api/core";
import type { PdfAnnotation, AnnotationColor, InkStroke } from "../../../types/pdf";

export function useDrawing(
  vaultPath: string | undefined,
  notePath: string,
  setAnnotations: React.Dispatch<React.SetStateAction<PdfAnnotation[]>>,
  saveAnnotations: (vaultPath: string, filePath: string, data: PdfAnnotation[]) => Promise<void>,
) {
  const [drawingMode, setDrawingMode] = useState(false);
  const [drawingColor, setDrawingColor] = useState<AnnotationColor>("blue");
  const [drawingStrokeWidth, setDrawingStrokeWidth] = useState(4);
  const pendingStrokesRef = useRef<Map<number, InkStroke[]>>(new Map());

  const commitPendingStrokes = useCallback(() => {
    if (!vaultPath) return;
    const pending = pendingStrokesRef.current;
    if (pending.size === 0) return;
    setAnnotations((prev) => {
      const next = [...prev];
      for (const [pageIdx, strokes] of pending.entries()) {
        if (strokes.length === 0) continue;
        const now = new Date().toISOString();
        next.push({
          id: crypto.randomUUID(), pageNumber: pageIdx + 1,
          type: "ink", color: drawingColor, inkStrokes: strokes,
          createdAt: now, updatedAt: now,
        });
      }
      void saveAnnotations(vaultPath, notePath, next);
      return next;
    });
    pendingStrokesRef.current = new Map();
  }, [vaultPath, notePath, drawingColor, saveAnnotations, setAnnotations]);

  const toggleDrawing = useCallback(() => {
    setDrawingMode((prev) => { if (prev) commitPendingStrokes(); return !prev; });
  }, [commitPendingStrokes]);

  const handleStrokeComplete = useCallback(async (pageIndex: number, stroke: InkStroke) => {
    try {
      const smoothed = await invoke<Array<{ points: Array<{ x: number; y: number; pressure: number }>; strokeWidth: number }>>(
        "smooth_ink_strokes", { strokes: [{ points: stroke.points, strokeWidth: stroke.strokeWidth }] },
      );
      const s: InkStroke = smoothed[0] ?? stroke;
      const pending = pendingStrokesRef.current;
      pending.set(pageIndex, [...(pending.get(pageIndex) ?? []), s]);
    } catch {
      const pending = pendingStrokesRef.current;
      pending.set(pageIndex, [...(pending.get(pageIndex) ?? []), stroke]);
    }
  }, []);

  return {
    drawingMode, setDrawingMode,
    drawingColor, setDrawingColor,
    drawingStrokeWidth, setDrawingStrokeWidth,
    toggleDrawing, handleStrokeComplete, commitPendingStrokes,
  };
}
