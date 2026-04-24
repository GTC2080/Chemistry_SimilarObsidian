import { useMemo } from "react";
import type { PdfAnnotation } from "../../types/pdf";
import { HIGHLIGHT_COLORS } from "../../types/pdf";

interface PdfAnnotationPanelProps {
  annotations: PdfAnnotation[];
  onClose: () => void;
  onNavigate: (pageNumber: number) => void;
  onDelete: (id: string) => void;
}

const TYPE_LABELS: Record<string, string> = {
  highlight: "\u9AD8\u4EAE",
  note: "\u6279\u6CE8",
  area: "\u533A\u57DF",
  ink: "\u7B14\u8FF9",
};

export default function PdfAnnotationPanel({
  annotations,
  onClose,
  onNavigate,
  onDelete,
}: PdfAnnotationPanelProps) {
  // Group annotations by page
  const grouped = useMemo(() => {
    const map = new Map<number, PdfAnnotation[]>();
    for (const a of annotations) {
      const list = map.get(a.pageNumber) ?? [];
      list.push(a);
      map.set(a.pageNumber, list);
    }
    return Array.from(map.entries()).sort(([a], [b]) => a - b);
  }, [annotations]);

  return (
    <div className="pdf-annotation-panel">
      <div className="pdf-annotation-panel-header">
        <span>
          {"\u6279\u6CE8"} ({annotations.length})
        </span>
        <button
          type="button"
          className="pdf-toolbar-btn"
          onClick={onClose}
          title="Close"
        >
          <svg width="14" height="14" viewBox="0 0 24 24" fill="none" stroke="currentColor" strokeWidth="2" strokeLinecap="round" strokeLinejoin="round"><line x1="18" y1="6" x2="6" y2="18"/><line x1="6" y1="6" x2="18" y2="18"/></svg>
        </button>
      </div>

      {annotations.length === 0 ? (
        <div className="pdf-annotation-panel-empty">
          {"\u6682\u65E0\u6279\u6CE8"}
        </div>
      ) : (
        <div className="pdf-annotation-panel-list">
          {grouped.map(([page, items]) => (
            <div key={page} className="pdf-annotation-panel-group">
              <div className="pdf-annotation-panel-page-label">
                {"\u7B2C"} {page} {"\u9875"}
              </div>
              {items.map((annotation) => (
                <div key={annotation.id} className="pdf-annotation-panel-item-row">
                  <button
                    type="button"
                    className="pdf-annotation-panel-item"
                    onClick={() => onNavigate(annotation.pageNumber)}
                  >
                    <span
                      className="pdf-annotation-panel-dot"
                      style={{
                        background: HIGHLIGHT_COLORS[annotation.color],
                      }}
                    />
                    <div className="pdf-annotation-panel-item-body">
                      <span className="pdf-annotation-panel-item-type">
                        {TYPE_LABELS[annotation.type] ?? annotation.type}
                      </span>
                      {annotation.selectedText && (
                        <span className="pdf-annotation-panel-item-text">
                          {annotation.selectedText.length > 60
                            ? annotation.selectedText.slice(0, 60) + "..."
                            : annotation.selectedText}
                        </span>
                      )}
                      {annotation.content && (
                        <span className="pdf-annotation-panel-item-note">
                          {annotation.content.length > 40
                            ? annotation.content.slice(0, 40) + "..."
                            : annotation.content}
                        </span>
                      )}
                    </div>
                  </button>
                  <button
                    type="button"
                    className="pdf-annotation-delete-btn"
                    title="删除"
                    onClick={(e) => { e.stopPropagation(); onDelete(annotation.id); }}
                  >
                    <svg width="12" height="12" viewBox="0 0 24 24" fill="none" stroke="currentColor" strokeWidth="2" strokeLinecap="round" strokeLinejoin="round"><polyline points="3 6 5 6 21 6"/><path d="M19 6v14a2 2 0 0 1-2 2H7a2 2 0 0 1-2-2V6m3 0V4a2 2 0 0 1 2-2h4a2 2 0 0 1 2 2v2"/></svg>
                  </button>
                </div>
              ))}
            </div>
          ))}
        </div>
      )}
    </div>
  );
}
