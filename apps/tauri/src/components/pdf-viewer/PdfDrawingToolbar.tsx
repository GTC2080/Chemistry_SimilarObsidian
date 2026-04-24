import type { AnnotationColor } from "../../types/pdf";
import { HIGHLIGHT_COLORS } from "../../types/pdf";

interface PdfDrawingToolbarProps {
  color: AnnotationColor;
  strokeWidth: number;
  onColorChange: (color: AnnotationColor) => void;
  onStrokeWidthChange: (width: number) => void;
  onClose: () => void;
}

const COLOR_ORDER: AnnotationColor[] = ["yellow", "red", "green", "blue", "purple"];
const WIDTH_OPTIONS = [2, 4, 6, 8];

export default function PdfDrawingToolbar({
  color,
  strokeWidth,
  onColorChange,
  onStrokeWidthChange,
  onClose,
}: PdfDrawingToolbarProps) {
  return (
    <div className="pdf-drawing-toolbar">
      {/* 颜色选择 */}
      {COLOR_ORDER.map((c) => (
        <button
          key={c}
          type="button"
          className={`pdf-drawing-color-btn ${c === color ? "pdf-drawing-color-active" : ""}`}
          style={{ background: HIGHLIGHT_COLORS[c] }}
          title={c}
          onClick={() => onColorChange(c)}
        />
      ))}

      <div className="pdf-toolbar-divider" />

      {/* 笔宽选择 */}
      {WIDTH_OPTIONS.map((w) => (
        <button
          key={w}
          type="button"
          className={`pdf-drawing-width-btn ${w === strokeWidth ? "pdf-drawing-width-active" : ""}`}
          title={`${w}px`}
          onClick={() => onStrokeWidthChange(w)}
        >
          <span
            className="pdf-drawing-width-dot"
            style={{ width: `${w + 2}px`, height: `${w + 2}px` }}
          />
        </button>
      ))}

      <div className="pdf-toolbar-divider" />

      {/* 退出绘图模式 */}
      <button
        type="button"
        className="pdf-toolbar-btn"
        onClick={onClose}
        title="退出绘图"
      >
        <svg width="14" height="14" viewBox="0 0 24 24" fill="none" stroke="currentColor" strokeWidth="2" strokeLinecap="round" strokeLinejoin="round">
          <line x1="18" y1="6" x2="6" y2="18" /><line x1="6" y1="6" x2="18" y2="18" />
        </svg>
      </button>
    </div>
  );
}
