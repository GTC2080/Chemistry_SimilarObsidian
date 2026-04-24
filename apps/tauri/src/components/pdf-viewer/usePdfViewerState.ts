/**
 * PDF Viewer 状态组合层
 *
 * 将 4 个子 hook 组合成统一接口供 PdfViewer 消费。
 * 各子 hook 按职责独立维护：
 *   - useViewerNav   — 导航、缩放、滚动、IO 观察
 *   - useAnnotations — 批注 CRUD、选区工具栏
 *   - useDrawing     — 绘图模式、笔画平滑
 *   - usePdfOutline  — 目录加载
 */

import { useCallback, useEffect, useState } from "react";
import type { SearchMatch } from "../../types/pdf";
import { perf } from "../../utils/perf";
import { usePdfLifecycle } from "../../hooks/usePdfRenderer";

import { useViewerNav, PAGE_BASE_WIDTH, PAGE_BASE_HEIGHT } from "./hooks/useViewerNav";
import { useAnnotations } from "./hooks/useAnnotations";
import { useDrawing } from "./hooks/useDrawing";
import { usePdfOutline } from "./hooks/usePdfOutline";

export { PAGE_BASE_WIDTH, PAGE_BASE_HEIGHT };

export function usePdfViewerState(noteId: string, notePath: string, vaultPath?: string) {
  const { docId, metadata, docHandle, openPdf, closePdf } = usePdfLifecycle();

  // --- Sub-hooks ---
  const nav = useViewerNav(noteId, metadata);
  const ann = useAnnotations(docId, notePath, vaultPath, nav.containerRef);
  const draw = useDrawing(vaultPath, notePath, ann.setAnnotations, ann.saveAnnotations);
  const outl = usePdfOutline(docId, docHandle?.doc ?? null);

  const [showSearch, setShowSearch] = useState(false);

  // --- PDF open / close ---
  useEffect(() => {
    let cancelled = false;
    nav.resetNav();
    ann.resetAnnotations();
    outl.resetOutline();
    setShowSearch(false);

    const endPdfOpen = perf.start("pdf-first-screen");
    void (async () => {
      try {
        await openPdf(notePath);
        if (!cancelled) { nav.setStatus("ready"); endPdfOpen(); }
      } catch (err) {
        if (!cancelled) {
          nav.setStatus("error");
          nav.setErrorMessage(err instanceof Error ? err.message : typeof err === "string" ? err : "Failed to open PDF");
        }
      }
    })();
    return () => { cancelled = true; void closePdf(); };
  }, [notePath, noteId, openPdf, closePdf]);

  // --- Keyboard shortcuts ---
  useEffect(() => {
    const handleKeyDown = (e: KeyboardEvent) => {
      const isCtrl = e.ctrlKey || e.metaKey;
      if (isCtrl && e.key === "f") { e.preventDefault(); setShowSearch((p) => !p); nav.resetToolbarTimer(); return; }
      if (e.key === "Escape") {
        if (ann.selectionToolbar) { ann.handleCopy; /* dismiss */ return; }
        if (draw.drawingMode) { draw.setDrawingMode(false); draw.commitPendingStrokes(); return; }
        if (showSearch) { setShowSearch(false); nav.resetToolbarTimer(); return; }
        return;
      }
      if (isCtrl && (e.key === "=" || e.key === "+")) { e.preventDefault(); nav.setZoom((p: number) => Math.min(4, Math.max(0.25, p + 0.25))); nav.resetToolbarTimer(); return; }
      if (isCtrl && e.key === "-") { e.preventDefault(); nav.setZoom((p: number) => Math.min(4, Math.max(0.25, p - 0.25))); nav.resetToolbarTimer(); return; }
      if (isCtrl && e.key === "0") { e.preventDefault(); nav.setZoom(1); nav.resetToolbarTimer(); return; }
    };
    const c = nav.containerRef.current;
    if (c) { c.addEventListener("keydown", handleKeyDown); return () => c.removeEventListener("keydown", handleKeyDown); }
  }, [showSearch, ann.selectionToolbar, draw.drawingMode, nav.resetToolbarTimer]);

  // --- Search handlers ---
  const toggleSearch = useCallback(() => setShowSearch((p) => !p), []);
  const handleSearchResults = useCallback((_r: SearchMatch[]) => {}, []);
  const handleSearchNavigate = useCallback((m: SearchMatch, _i: number) => { nav.handlePageChange(m.page + 1); }, [nav.handlePageChange]);
  const handleCloseSearch = useCallback(() => setShowSearch(false), []);

  // --- Wire annotation navigate to nav ---
  const handleAnnotationNavigate = useCallback((p: number) => nav.handlePageChange(p), [nav.handlePageChange]);
  const handleOutlineNavigate = useCallback((p: number) => nav.handlePageChange(p), [nav.handlePageChange]);

  return {
    // pdf.js
    docId, metadata, docHandle,
    // nav
    status: nav.status, errorMessage: nav.errorMessage,
    currentPage: nav.currentPage, zoom: nav.zoom, setZoom: nav.setZoom,
    visiblePages: nav.visiblePages, toolbarVisible: nav.toolbarVisible,
    containerRef: nav.containerRef, scrollRef: nav.scrollRef, setPageRef: nav.setPageRef,
    handleMouseMove: nav.handleMouseMove, handlePageChange: nav.handlePageChange,
    // search
    showSearch, toggleSearch,
    handleSearchResults, handleSearchNavigate, handleCloseSearch,
    // outline
    showOutline: outl.showOutline, outline: outl.outline,
    toggleOutline: outl.toggleOutline, handleOutlineClose: outl.handleOutlineClose,
    handleOutlineNavigate,
    // annotations
    annotations: ann.annotations, showAnnotationPanel: ann.showAnnotationPanel,
    selectionToolbar: ann.selectionToolbar, selectionToolbarRef: ann.selectionToolbarRef,
    toggleAnnotations: ann.toggleAnnotations,
    handleAnnotationNavigate, handleAnnotationPanelClose: ann.handleAnnotationPanelClose,
    handleDeleteAnnotation: ann.handleDeleteAnnotation,
    handleTextSelected: ann.handleTextSelected,
    handleHighlight: ann.handleHighlight, handleNote: ann.handleNote, handleCopy: ann.handleCopy,
    // drawing
    drawingMode: draw.drawingMode, drawingColor: draw.drawingColor,
    drawingStrokeWidth: draw.drawingStrokeWidth,
    setDrawingColor: draw.setDrawingColor, setDrawingStrokeWidth: draw.setDrawingStrokeWidth,
    toggleDrawing: draw.toggleDrawing, handleStrokeComplete: draw.handleStrokeComplete,
  };
}
