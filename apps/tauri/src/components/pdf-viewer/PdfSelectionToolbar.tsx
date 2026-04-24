import type { AnnotationColor } from "../../types/pdf";
import { HIGHLIGHT_COLORS } from "../../types/pdf";

interface PdfSelectionToolbarProps {
  /** Position in viewport pixels (relative to the pdf-viewer container) */
  x: number;
  y: number;
  onHighlight: (color: AnnotationColor) => void;
  onNote: () => void;
  onCopy: () => void;
}

const COLOR_ORDER: AnnotationColor[] = [
  "yellow",
  "red",
  "green",
  "blue",
  "purple",
];

export default function PdfSelectionToolbar({
  x,
  y,
  onHighlight,
  onNote,
  onCopy,
}: PdfSelectionToolbarProps) {
  return (
    <div
      className="pdf-selection-toolbar"
      style={{ left: `${x}px`, top: `${y}px` }}
    >
      {COLOR_ORDER.map((color) => (
        <button
          key={color}
          type="button"
          className="pdf-selection-color-btn"
          style={{ background: HIGHLIGHT_COLORS[color] }}
          title={color}
          onClick={() => onHighlight(color)}
        />
      ))}
      <div className="pdf-selection-divider" />
      <button
        type="button"
        className="pdf-selection-action-btn"
        title="Add Note"
        onClick={onNote}
      >
        <svg width="14" height="14" viewBox="0 0 24 24" fill="none" stroke="currentColor" strokeWidth="2" strokeLinecap="round" strokeLinejoin="round"><path d="M14 2H6a2 2 0 0 0-2 2v16a2 2 0 0 0 2 2h12a2 2 0 0 0 2-2V8z"/><polyline points="14 2 14 8 20 8"/><line x1="16" y1="13" x2="8" y2="13"/><line x1="16" y1="17" x2="8" y2="17"/></svg>
      </button>
      <button
        type="button"
        className="pdf-selection-action-btn"
        title="Copy"
        onClick={onCopy}
      >
        <svg width="14" height="14" viewBox="0 0 24 24" fill="none" stroke="currentColor" strokeWidth="2" strokeLinecap="round" strokeLinejoin="round"><rect x="9" y="9" width="13" height="13" rx="2" ry="2"/><path d="M5 15H4a2 2 0 0 1-2-2V4a2 2 0 0 1 2-2h9a2 2 0 0 1 2 2v1"/></svg>
      </button>
    </div>
  );
}
