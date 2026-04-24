import { useCallback } from "react";
import type { OutlineEntry } from "../../types/pdf";

interface PdfOutlinePanelProps {
  outline: OutlineEntry[];
  onNavigate: (pageNumber: number) => void;
  onClose: () => void;
}

function OutlineItem({
  entry,
  depth,
  onNavigate,
  onClose,
}: {
  entry: OutlineEntry;
  depth: number;
  onNavigate: (pageNumber: number) => void;
  onClose: () => void;
}) {
  const handleClick = useCallback(() => {
    if (entry.page != null) {
      // Outline pages are 0-based; navigate expects 1-based page
      onNavigate(entry.page + 1);
      onClose();
    }
  }, [entry.page, onNavigate, onClose]);

  return (
    <>
      <button
        type="button"
        className="pdf-outline-item"
        style={{ paddingLeft: `${16 + depth * 16}px` }}
        onClick={handleClick}
        disabled={entry.page == null}
        title={entry.title}
      >
        {entry.title}
      </button>
      {entry.children.map((child, i) => (
        <OutlineItem
          key={i}
          entry={child}
          depth={depth + 1}
          onNavigate={onNavigate}
          onClose={onClose}
        />
      ))}
    </>
  );
}

export default function PdfOutlinePanel({
  outline,
  onNavigate,
  onClose,
}: PdfOutlinePanelProps) {
  return (
    <div className="pdf-outline-panel">
      <div className="pdf-outline-header">
        {"\u76EE\u5F55"}
      </div>

      {outline.length === 0 ? (
        <div className="pdf-outline-empty">
          {"\u6B64 PDF \u6CA1\u6709\u76EE\u5F55"}
        </div>
      ) : (
        outline.map((entry, i) => (
          <OutlineItem
            key={i}
            entry={entry}
            depth={0}
            onNavigate={onNavigate}
            onClose={onClose}
          />
        ))
      )}
    </div>
  );
}
