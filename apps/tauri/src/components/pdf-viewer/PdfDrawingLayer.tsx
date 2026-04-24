import { memo, useCallback, useEffect, useRef } from "react";
import type { InkPoint, InkStroke, AnnotationColor } from "../../types/pdf";

interface PdfDrawingLayerProps {
  /** 是否处于绘图模式 */
  active: boolean;
  /** 画笔颜色 */
  color: AnnotationColor;
  /** 画笔宽度（像素） */
  strokeWidth: number;
  /** 页面显示宽度（像素） */
  displayWidth: number;
  /** 页面显示高度（像素） */
  displayHeight: number;
  /** 当一条笔画完成时回调（pen-up），传出归一化坐标点 */
  onStrokeComplete: (stroke: InkStroke) => void;
}

/** 将 AnnotationColor 转为不透明的 CSS 颜色（绘图时用实色） */
function colorToSolid(color: AnnotationColor): string {
  const map: Record<AnnotationColor, string> = {
    yellow: "#F5C518",
    red: "#FF453A",
    green: "#32D74B",
    blue: "#0A84FF",
    purple: "#BF5AF2",
  };
  return map[color];
}

const PdfDrawingLayer = memo(function PdfDrawingLayer({
  active,
  color,
  strokeWidth,
  displayWidth,
  displayHeight,
  onStrokeComplete,
}: PdfDrawingLayerProps) {
  const canvasRef = useRef<HTMLCanvasElement>(null);
  const drawingRef = useRef(false);
  const pointsRef = useRef<InkPoint[]>([]);

  // 获取归一化坐标
  const toNorm = useCallback(
    (e: PointerEvent): InkPoint => {
      const canvas = canvasRef.current!;
      const rect = canvas.getBoundingClientRect();
      return {
        x: (e.clientX - rect.left) / rect.width,
        y: (e.clientY - rect.top) / rect.height,
        pressure: e.pressure > 0 ? e.pressure : 0.5,
      };
    },
    [],
  );

  // 在 canvas 上画线段
  const drawSegment = useCallback(
    (from: InkPoint, to: InkPoint) => {
      const canvas = canvasRef.current;
      if (!canvas) return;
      const ctx = canvas.getContext("2d");
      if (!ctx) return;

      const w = canvas.width;
      const h = canvas.height;

      ctx.strokeStyle = colorToSolid(color);
      ctx.lineWidth = strokeWidth * (to.pressure + 0.5);
      ctx.lineCap = "round";
      ctx.lineJoin = "round";

      ctx.beginPath();
      ctx.moveTo(from.x * w, from.y * h);
      ctx.lineTo(to.x * w, to.y * h);
      ctx.stroke();
    },
    [color, strokeWidth],
  );

  useEffect(() => {
    const canvas = canvasRef.current;
    if (!canvas || !active) return;

    const onPointerDown = (e: PointerEvent) => {
      if (e.button !== 0) return; // 只响应左键/主触点
      drawingRef.current = true;
      pointsRef.current = [toNorm(e)];
      canvas.setPointerCapture(e.pointerId);
    };

    const onPointerMove = (e: PointerEvent) => {
      if (!drawingRef.current) return;
      const pt = toNorm(e);
      const prev = pointsRef.current[pointsRef.current.length - 1];
      pointsRef.current.push(pt);
      drawSegment(prev, pt);
    };

    const onPointerUp = (e: PointerEvent) => {
      if (!drawingRef.current) return;
      drawingRef.current = false;
      canvas.releasePointerCapture(e.pointerId);

      if (pointsRef.current.length >= 2) {
        onStrokeComplete({
          points: pointsRef.current,
          strokeWidth: strokeWidth / displayWidth, // 归一化
        });
      }
      pointsRef.current = [];
    };

    canvas.addEventListener("pointerdown", onPointerDown);
    canvas.addEventListener("pointermove", onPointerMove);
    canvas.addEventListener("pointerup", onPointerUp);
    canvas.addEventListener("pointercancel", onPointerUp);

    return () => {
      canvas.removeEventListener("pointerdown", onPointerDown);
      canvas.removeEventListener("pointermove", onPointerMove);
      canvas.removeEventListener("pointerup", onPointerUp);
      canvas.removeEventListener("pointercancel", onPointerUp);
    };
  }, [active, toNorm, drawSegment, onStrokeComplete, strokeWidth, displayWidth]);

  // 清空 canvas（当颜色/笔宽变化或退出绘图模式时）
  useEffect(() => {
    const canvas = canvasRef.current;
    if (!canvas) return;
    const ctx = canvas.getContext("2d");
    if (ctx) ctx.clearRect(0, 0, canvas.width, canvas.height);
  }, [active]);

  if (!active) return null;

  const dpr = window.devicePixelRatio || 1;

  return (
    <canvas
      ref={canvasRef}
      className="pdf-drawing-layer"
      width={Math.floor(displayWidth * dpr)}
      height={Math.floor(displayHeight * dpr)}
      style={{
        width: `${displayWidth}px`,
        height: `${displayHeight}px`,
        cursor: "crosshair",
      }}
    />
  );
});

export default PdfDrawingLayer;
