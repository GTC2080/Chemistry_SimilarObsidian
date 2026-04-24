import { memo } from "react";
import type { PdfAnnotation, InkStroke } from "../../types/pdf";
import { HIGHLIGHT_COLORS } from "../../types/pdf";

interface PdfAnnotationLayerProps {
  pageIndex: number;
  annotations: PdfAnnotation[];
  onAnnotationClick?: (annotation: PdfAnnotation) => void;
}

/** 将归一化笔迹点序列转为 SVG path d 属性 */
function inkStrokeToPath(stroke: InkStroke): string {
  const pts = stroke.points;
  if (pts.length === 0) return "";
  let d = `M ${pts[0].x * 100} ${pts[0].y * 100}`;
  for (let i = 1; i < pts.length; i++) {
    d += ` L ${pts[i].x * 100} ${pts[i].y * 100}`;
  }
  return d;
}

/** 将 AnnotationColor 转为实色（ink 用） */
const INK_SOLID_COLORS: Record<string, string> = {
  yellow: "#F5C518",
  red: "#FF453A",
  green: "#32D74B",
  blue: "#0A84FF",
  purple: "#BF5AF2",
};

const PdfAnnotationLayer = memo(function PdfAnnotationLayer({
  pageIndex,
  annotations,
  onAnnotationClick,
}: PdfAnnotationLayerProps) {
  // Annotations use 1-based pageNumber; pageIndex is 0-based
  const pageAnnotations = annotations.filter(
    (a) => a.pageNumber === pageIndex + 1,
  );

  if (pageAnnotations.length === 0) {
    return null;
  }

  return (
    <div className="pdf-annotation-layer">
      {pageAnnotations.map((annotation) => {
        // --- Ink annotation: render as SVG ---
        if (annotation.type === "ink" && annotation.inkStrokes) {
          return (
            <svg
              key={annotation.id}
              className="pdf-annotation-ink"
              viewBox="0 0 100 100"
              preserveAspectRatio="none"
              onClick={() => onAnnotationClick?.(annotation)}
            >
              {annotation.inkStrokes.map((stroke, si) => (
                <path
                  key={si}
                  d={inkStrokeToPath(stroke)}
                  fill="none"
                  stroke={INK_SOLID_COLORS[annotation.color] ?? "#0A84FF"}
                  strokeWidth={stroke.strokeWidth * 100}
                  strokeLinecap="round"
                  strokeLinejoin="round"
                  vectorEffect="non-scaling-stroke"
                />
              ))}
            </svg>
          );
        }

        // --- Area annotation ---
        if (annotation.type === "area" && annotation.area) {
          const r = annotation.area;
          return (
            <div
              key={annotation.id}
              className="pdf-annotation-highlight pdf-annotation-area"
              style={{
                left: `${r.x * 100}%`,
                top: `${r.y * 100}%`,
                width: `${r.w * 100}%`,
                height: `${r.h * 100}%`,
                border: `2px dashed ${HIGHLIGHT_COLORS[annotation.color]}`,
                background: "transparent",
              }}
              onClick={() => onAnnotationClick?.(annotation)}
            />
          );
        }

        // --- Text highlight ---
        if (annotation.textRanges) {
          return annotation.textRanges.map((range, ri) =>
            range.rects.map((rect, rj) => (
              <div
                key={`${annotation.id}-${ri}-${rj}`}
                className="pdf-annotation-highlight"
                style={{
                  left: `${rect.x * 100}%`,
                  top: `${rect.y * 100}%`,
                  width: `${rect.w * 100}%`,
                  height: `${rect.h * 100}%`,
                  background: HIGHLIGHT_COLORS[annotation.color],
                }}
                onClick={() => onAnnotationClick?.(annotation)}
              />
            )),
          );
        }

        return null;
      })}
    </div>
  );
});

export default PdfAnnotationLayer;
