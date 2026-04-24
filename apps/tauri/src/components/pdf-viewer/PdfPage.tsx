import { memo, useEffect, useRef, useState } from "react";
import type { PdfAnnotation, InkStroke, AnnotationColor } from "../../types/pdf";
import { usePdfRenderer } from "../../hooks/usePdfRenderer";
import PdfTextLayer from "./PdfTextLayer";
import type { TextSelectionInfo } from "./PdfTextLayer";
import PdfAnnotationLayer from "./PdfAnnotationLayer";
import PdfDrawingLayer from "./PdfDrawingLayer";

interface PdfPageProps {
  pageIndex: number;
  widthPts: number;
  heightPts: number;
  zoom: number;
  isVisible: boolean;
  annotations: PdfAnnotation[];
  onAnnotationClick?: (annotation: PdfAnnotation) => void;
  onTextSelected?: (info: TextSelectionInfo) => void;
  /** 绘图模式 */
  drawingMode: boolean;
  drawingColor: AnnotationColor;
  drawingStrokeWidth: number;
  onStrokeComplete?: (pageIndex: number, stroke: InkStroke) => void;
}

const PdfPage = memo(function PdfPage({
  pageIndex,
  widthPts,
  heightPts,
  zoom,
  isVisible,
  annotations,
  onAnnotationClick,
  onTextSelected,
  drawingMode,
  drawingColor,
  drawingStrokeWidth,
  onStrokeComplete,
}: PdfPageProps) {
  const { renderToCanvas } = usePdfRenderer();
  const canvasRef = useRef<HTMLCanvasElement>(null);
  const [loading, setLoading] = useState(false);
  const [rendered, setRendered] = useState(false);
  const renderKeyRef = useRef(0);

  const displayWidth = widthPts * zoom;
  const displayHeight = heightPts * zoom;

  // Debounce render when zoom changes rapidly (Ctrl+scroll).
  const zoomTimerRef = useRef<ReturnType<typeof setTimeout> | null>(null);
  const prevZoomRef = useRef(zoom);

  useEffect(() => {
    if (!isVisible || !canvasRef.current) {
      return;
    }

    const doRender = () => {
      const key = ++renderKeyRef.current;
      const scale = zoom * window.devicePixelRatio;
      const canvas = canvasRef.current;
      if (!canvas) return;

      setLoading(true);
      renderToCanvas(pageIndex, scale, canvas)
        .then(() => {
          if (renderKeyRef.current === key) {
            setRendered(true);
            setLoading(false);
          }
        })
        .catch(() => {
          if (renderKeyRef.current === key) {
            setLoading(false);
          }
        });
    };

    // If only zoom changed, debounce to avoid rendering every intermediate step.
    if (prevZoomRef.current !== zoom) {
      prevZoomRef.current = zoom;
      if (zoomTimerRef.current) clearTimeout(zoomTimerRef.current);
      zoomTimerRef.current = setTimeout(doRender, 150);
    } else {
      doRender();
    }

    return () => {
      if (zoomTimerRef.current) clearTimeout(zoomTimerRef.current);
    };
  }, [isVisible, pageIndex, zoom, renderToCanvas]);

  const handleStrokeComplete = (stroke: InkStroke) => {
    onStrokeComplete?.(pageIndex, stroke);
  };

  return (
    <div
      className="pdf-page-wrapper"
      style={{ width: `${displayWidth}px`, height: `${displayHeight}px` }}
    >
      <canvas
        ref={canvasRef}
        className="pdf-page-canvas"
        style={{
          width: `${displayWidth}px`,
          height: `${displayHeight}px`,
          display: isVisible ? "block" : "none",
        }}
      />

      {!isVisible || !rendered ? (
        <div
          className="pdf-page-placeholder"
          style={{ width: `${displayWidth}px`, height: `${displayHeight}px` }}
        >
          {loading ? "" : pageIndex + 1}
        </div>
      ) : null}

      {isVisible && (
        <>
          {!drawingMode && (
            <PdfTextLayer pageIndex={pageIndex} isVisible={isVisible} onTextSelected={onTextSelected} />
          )}
          <PdfAnnotationLayer
            pageIndex={pageIndex}
            annotations={annotations}
            onAnnotationClick={onAnnotationClick}
          />
          <PdfDrawingLayer
            active={drawingMode}
            color={drawingColor}
            strokeWidth={drawingStrokeWidth}
            displayWidth={displayWidth}
            displayHeight={displayHeight}
            onStrokeComplete={handleStrokeComplete}
          />
        </>
      )}
    </div>
  );
});

export default PdfPage;
