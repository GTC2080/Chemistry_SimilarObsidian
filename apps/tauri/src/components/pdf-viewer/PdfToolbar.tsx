import { useCallback, useRef, useState } from "react";
import type { PdfMetadata } from "../../types/pdf";

interface PdfToolbarProps {
  metadata: PdfMetadata | null;
  currentPage: number;
  zoom: number;
  showOutline: boolean;
  visible: boolean;
  drawingMode: boolean;
  onPageChange: (page: number) => void;
  onZoomChange: (zoom: number) => void;
  onToggleSearch: () => void;
  onToggleOutline: () => void;
  onToggleAnnotations: () => void;
  onToggleDrawing: () => void;
}

const ZOOM_MIN = 0.25;
const ZOOM_MAX = 5;
const ZOOM_STEP = 0.15;

function clampZoom(value: number): number {
  return Math.min(ZOOM_MAX, Math.max(ZOOM_MIN, value));
}

export default function PdfToolbar({
  metadata,
  currentPage,
  zoom,
  showOutline,
  visible,
  drawingMode,
  onPageChange,
  onZoomChange,
  onToggleSearch,
  onToggleOutline,
  onToggleAnnotations,
  onToggleDrawing,
}: PdfToolbarProps) {
  const pageCount = metadata?.page_count ?? 0;
  const [editingPage, setEditingPage] = useState(false);
  const inputRef = useRef<HTMLInputElement>(null);

  const handlePageInputCommit = useCallback(
    (value: string) => {
      setEditingPage(false);
      const num = parseInt(value, 10);
      if (!Number.isNaN(num) && num >= 1 && num <= pageCount) {
        onPageChange(num);
      }
    },
    [onPageChange, pageCount],
  );

  const handlePageInputKeyDown = useCallback(
    (e: React.KeyboardEvent<HTMLInputElement>) => {
      if (e.key === "Enter") {
        handlePageInputCommit(e.currentTarget.value);
      } else if (e.key === "Escape") {
        setEditingPage(false);
      }
    },
    [handlePageInputCommit],
  );

  const hasOutline = metadata?.outline && metadata.outline.length > 0;

  return (
    <div className={`pdf-toolbar-wrapper ${visible ? "" : "pdf-toolbar-hidden"}`}>
      <div className="pdf-toolbar">
        {/* TOC / Outline */}
        <button
          type="button"
          className={`pdf-toolbar-btn ${showOutline ? "pdf-toolbar-btn-active" : ""}`}
          onClick={onToggleOutline}
          disabled={!hasOutline}
          title="Table of Contents"
        >
          <svg width="16" height="16" viewBox="0 0 24 24" fill="none" stroke="currentColor" strokeWidth="2" strokeLinecap="round" strokeLinejoin="round"><line x1="3" y1="6" x2="21" y2="6"/><line x1="3" y1="12" x2="21" y2="12"/><line x1="3" y1="18" x2="21" y2="18"/></svg>
        </button>

        <div className="pdf-toolbar-divider" />

        {/* Zoom controls */}
        <button
          type="button"
          className="pdf-toolbar-btn"
          onClick={() => onZoomChange(clampZoom(zoom - ZOOM_STEP))}
          disabled={zoom <= ZOOM_MIN}
          title="Zoom Out"
        >
          &minus;
        </button>
        <span className="pdf-toolbar-zoom">
          {Math.round(zoom * 100)}%
        </span>
        <button
          type="button"
          className="pdf-toolbar-btn"
          onClick={() => onZoomChange(clampZoom(zoom + ZOOM_STEP))}
          disabled={zoom >= ZOOM_MAX}
          title="Zoom In"
        >
          +
        </button>

        <div className="pdf-toolbar-divider" />

        {/* Search */}
        <button
          type="button"
          className="pdf-toolbar-btn"
          onClick={onToggleSearch}
          title="Search (Ctrl+F)"
        >
          <svg width="16" height="16" viewBox="0 0 24 24" fill="none" stroke="currentColor" strokeWidth="2" strokeLinecap="round" strokeLinejoin="round"><circle cx="11" cy="11" r="8"/><line x1="21" y1="21" x2="16.65" y2="16.65"/></svg>
        </button>

        <div className="pdf-toolbar-divider" />

        {/* Page navigation */}
        <button
          type="button"
          className="pdf-toolbar-btn"
          onClick={() => onPageChange(Math.max(1, currentPage - 1))}
          disabled={currentPage <= 1}
          title="Previous Page"
        >
          <svg width="14" height="14" viewBox="0 0 24 24" fill="none" stroke="currentColor" strokeWidth="2" strokeLinecap="round" strokeLinejoin="round"><polyline points="15 18 9 12 15 6"/></svg>
        </button>

        <div className="pdf-toolbar-page">
          {editingPage ? (
            <input
              ref={inputRef}
              type="text"
              className="pdf-toolbar-page-input"
              defaultValue={String(currentPage)}
              autoFocus
              onBlur={e => handlePageInputCommit(e.currentTarget.value)}
              onKeyDown={handlePageInputKeyDown}
            />
          ) : (
            <button
              type="button"
              className="pdf-toolbar-page-input"
              style={{ cursor: "text" }}
              onClick={() => {
                setEditingPage(true);
                // Focus will happen via autoFocus on the input
              }}
            >
              {currentPage}
            </button>
          )}
          <span className="pdf-toolbar-page-total">/ {pageCount}</span>
        </div>

        <button
          type="button"
          className="pdf-toolbar-btn"
          onClick={() => onPageChange(Math.min(pageCount, currentPage + 1))}
          disabled={currentPage >= pageCount}
          title="Next Page"
        >
          <svg width="14" height="14" viewBox="0 0 24 24" fill="none" stroke="currentColor" strokeWidth="2" strokeLinecap="round" strokeLinejoin="round"><polyline points="9 18 15 12 9 6"/></svg>
        </button>

        <div className="pdf-toolbar-divider" />

        {/* Drawing / Pen */}
        <button
          type="button"
          className={`pdf-toolbar-btn ${drawingMode ? "pdf-toolbar-btn-active" : ""}`}
          onClick={onToggleDrawing}
          title="Draw (D)"
        >
          <svg width="16" height="16" viewBox="0 0 24 24" fill="none" stroke="currentColor" strokeWidth="2" strokeLinecap="round" strokeLinejoin="round"><path d="M12 19l7-7 3 3-7 7-3-3z"/><path d="M18 13l-1.5-7.5L2 2l3.5 14.5L13 18l5-5z"/><path d="M2 2l7.586 7.586"/><circle cx="11" cy="11" r="2"/></svg>
        </button>

        {/* Annotations */}
        <button
          type="button"
          className="pdf-toolbar-btn"
          onClick={onToggleAnnotations}
          title="Annotations"
        >
          <svg width="16" height="16" viewBox="0 0 24 24" fill="none" stroke="currentColor" strokeWidth="2" strokeLinecap="round" strokeLinejoin="round"><path d="M14 2H6a2 2 0 0 0-2 2v16a2 2 0 0 0 2 2h12a2 2 0 0 0 2-2V8z"/><polyline points="14 2 14 8 20 8"/><line x1="16" y1="13" x2="8" y2="13"/><line x1="16" y1="17" x2="8" y2="17"/></svg>
        </button>
      </div>
    </div>
  );
}
