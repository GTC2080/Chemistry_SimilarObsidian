/**
 * PDF Viewer — 纯渲染组件
 *
 * 所有状态管理和事件逻辑在 usePdfViewerState hook 中，
 * 这里只负责组装子组件。
 */

import { useMemo } from "react";
import type { NoteInfo } from "../../types";
import { PdfDocContext } from "../../hooks/usePdfRenderer";
import PdfToolbar from "./PdfToolbar";
import PdfPage from "./PdfPage";
import PdfSearchBar from "./PdfSearchBar";
import PdfOutlinePanel from "./PdfOutlinePanel";
import PdfAnnotationPanel from "./PdfAnnotationPanel";
import PdfSelectionToolbar from "./PdfSelectionToolbar";
import PdfDrawingToolbar from "./PdfDrawingToolbar";
import { usePdfViewerState, PAGE_BASE_WIDTH, PAGE_BASE_HEIGHT } from "./usePdfViewerState";
import "./pdf-viewer.css";

interface PdfViewerProps {
  note: NoteInfo;
  vaultPath?: string;
}

export default function PdfViewer({ note, vaultPath }: PdfViewerProps) {
  const s = usePdfViewerState(note.id, note.path, vaultPath);

  // --- Page list ---
  const pageElements = useMemo(() => {
    if (!s.metadata) return null;
    return Array.from({ length: s.metadata.page_count }, (_, i) => {
      const dim = s.metadata!.page_dimensions[i];
      const w = dim?.width ?? PAGE_BASE_WIDTH;
      const h = dim?.height ?? PAGE_BASE_HEIGHT;
      return (
        <div key={i} data-page-index={i} ref={(el) => s.setPageRef(i, el)}>
          <PdfPage
            pageIndex={i}
            widthPts={w}
            heightPts={h}
            zoom={s.zoom}
            isVisible={s.visiblePages.has(i)}
            annotations={s.annotations}
            onTextSelected={s.handleTextSelected}
            drawingMode={s.drawingMode}
            drawingColor={s.drawingColor}
            drawingStrokeWidth={s.drawingStrokeWidth}
            onStrokeComplete={s.handleStrokeComplete}
          />
        </div>
      );
    });
  }, [s.metadata, s.zoom, s.visiblePages, s.annotations, s.setPageRef, s.handleTextSelected, s.drawingMode, s.drawingColor, s.drawingStrokeWidth, s.handleStrokeComplete]);

  // --- Error ---
  if (s.status === "error") {
    return (
      <div className="pdf-viewer">
        <div className="pdf-viewer-error">
          <span className="pdf-viewer-error-icon">
            <svg width="24" height="24" viewBox="0 0 24 24" fill="none" stroke="currentColor" strokeWidth="2" strokeLinecap="round" strokeLinejoin="round"><path d="M10.29 3.86L1.82 18a2 2 0 0 0 1.71 3h16.94a2 2 0 0 0 1.71-3L13.71 3.86a2 2 0 0 0-3.42 0z"/><line x1="12" y1="9" x2="12" y2="13"/><line x1="12" y1="17" x2="12.01" y2="17"/></svg>
          </span>
          <span className="pdf-viewer-error-message">{s.errorMessage}</span>
        </div>
      </div>
    );
  }

  // --- Loading ---
  if (s.status === "loading") {
    return (
      <div className="pdf-viewer">
        <div className="pdf-viewer-loading">
          <div className="pdf-viewer-loading-spinner" />
        </div>
      </div>
    );
  }

  // --- Ready ---
  return (
    <PdfDocContext.Provider value={s.docHandle}>
      <div ref={s.containerRef} className="pdf-viewer" onMouseMove={s.handleMouseMove} tabIndex={-1}>
        <div ref={s.scrollRef} className="pdf-viewer-scroll">
          {pageElements}
        </div>

        {s.showSearch && (
          <PdfSearchBar onResults={s.handleSearchResults} onNavigate={s.handleSearchNavigate} onClose={s.handleCloseSearch} />
        )}

        {s.showOutline && (
          <PdfOutlinePanel outline={s.outline} onNavigate={s.handleOutlineNavigate} onClose={s.handleOutlineClose} />
        )}

        {s.showAnnotationPanel && (
          <PdfAnnotationPanel annotations={s.annotations} onClose={s.handleAnnotationPanelClose} onNavigate={s.handleAnnotationNavigate} onDelete={s.handleDeleteAnnotation} />
        )}

        {s.selectionToolbar && (
          <div ref={s.selectionToolbarRef}>
            <PdfSelectionToolbar x={s.selectionToolbar.x} y={s.selectionToolbar.y} onHighlight={s.handleHighlight} onNote={s.handleNote} onCopy={s.handleCopy} />
          </div>
        )}

        {s.drawingMode && (
          <PdfDrawingToolbar color={s.drawingColor} strokeWidth={s.drawingStrokeWidth} onColorChange={s.setDrawingColor} onStrokeWidthChange={s.setDrawingStrokeWidth} onClose={s.toggleDrawing} />
        )}

        <PdfToolbar
          metadata={s.metadata}
          currentPage={s.currentPage}
          zoom={s.zoom}
          showOutline={s.showOutline}
          visible={s.toolbarVisible}
          drawingMode={s.drawingMode}
          onPageChange={s.handlePageChange}
          onZoomChange={s.setZoom}
          onToggleSearch={s.toggleSearch}
          onToggleOutline={s.toggleOutline}
          onToggleAnnotations={s.toggleAnnotations}
          onToggleDrawing={s.toggleDrawing}
        />
      </div>
    </PdfDocContext.Provider>
  );
}
