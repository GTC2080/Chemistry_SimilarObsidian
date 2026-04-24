/**
 * 批注 CRUD：加载、高亮创建、删除、选区工具栏
 */

import { useCallback, useEffect, useRef, useState } from "react";
import type { PdfAnnotation, AnnotationColor } from "../../../types/pdf";
import { usePdfAnnotations } from "../../../hooks/usePdfRenderer";
import type { TextSelectionInfo } from "../PdfTextLayer";

export interface SelectionToolbarState {
  x: number;
  y: number;
  pageIndex: number;
  selectedText: string;
  textRanges: PdfAnnotation["textRanges"];
}

export function useAnnotations(
  docId: string | null,
  notePath: string,
  vaultPath: string | undefined,
  containerRef: React.RefObject<HTMLDivElement | null>,
) {
  const { loadAnnotations, saveAnnotations } = usePdfAnnotations();
  const [annotations, setAnnotations] = useState<PdfAnnotation[]>([]);
  const [showAnnotationPanel, setShowAnnotationPanel] = useState(false);
  const [selectionToolbar, setSelectionToolbar] = useState<SelectionToolbarState | null>(null);
  const selectionToolbarRef = useRef<HTMLDivElement>(null);

  // --- Load on open ---
  useEffect(() => {
    if (!docId || !vaultPath) return;
    let cancelled = false;
    void (async () => {
      try {
        const loaded = await loadAnnotations(vaultPath, notePath);
        if (!cancelled) setAnnotations(loaded);
      } catch { /* optional */ }
    })();
    return () => { cancelled = true; };
  }, [docId, vaultPath, notePath, loadAnnotations]);

  // --- Selection toolbar dismissal ---
  useEffect(() => {
    if (!selectionToolbar) return;
    const handler = (e: MouseEvent) => {
      if (selectionToolbarRef.current?.contains(e.target as Node)) return;
      setSelectionToolbar(null);
    };
    const timer = setTimeout(() => document.addEventListener("mousedown", handler), 50);
    return () => { clearTimeout(timer); document.removeEventListener("mousedown", handler); };
  }, [selectionToolbar]);

  // --- Text selected ---
  const handleTextSelected = useCallback((info: TextSelectionInfo) => {
    if (!containerRef.current) return;
    const cr = containerRef.current.getBoundingClientRect();
    setSelectionToolbar({
      x: info.clientX - cr.left,
      y: info.clientY - cr.top - 40,
      pageIndex: info.pageIndex,
      selectedText: info.selectedText,
      textRanges: info.textRanges,
    });
  }, [containerRef]);

  // --- Highlight ---
  const handleHighlight = useCallback((color: AnnotationColor) => {
    if (!selectionToolbar || !vaultPath) return;
    const now = new Date().toISOString();
    const a: PdfAnnotation = {
      id: crypto.randomUUID(),
      pageNumber: selectionToolbar.pageIndex + 1,
      type: "highlight", color,
      textRanges: selectionToolbar.textRanges,
      selectedText: selectionToolbar.selectedText,
      createdAt: now, updatedAt: now,
    };
    setAnnotations((prev) => { const next = [...prev, a]; void saveAnnotations(vaultPath, notePath, next); return next; });
    window.getSelection()?.removeAllRanges();
    setSelectionToolbar(null);
  }, [selectionToolbar, vaultPath, notePath, saveAnnotations]);

  const handleNote = useCallback(() => { if (selectionToolbar) handleHighlight("yellow"); }, [selectionToolbar, handleHighlight]);

  const handleCopy = useCallback(() => {
    if (!selectionToolbar) return;
    void navigator.clipboard.writeText(selectionToolbar.selectedText);
    window.getSelection()?.removeAllRanges();
    setSelectionToolbar(null);
  }, [selectionToolbar]);

  // --- Delete ---
  const handleDeleteAnnotation = useCallback((id: string) => {
    if (!vaultPath) return;
    setAnnotations((prev) => { const next = prev.filter((a) => a.id !== id); void saveAnnotations(vaultPath, notePath, next); return next; });
  }, [vaultPath, notePath, saveAnnotations]);

  // --- Toggle panel ---
  const toggleAnnotations = useCallback(() => setShowAnnotationPanel((p) => !p), []);
  const handleAnnotationNavigate = useCallback((_p: number) => {}, []); // wired by caller
  const handleAnnotationPanelClose = useCallback(() => setShowAnnotationPanel(false), []);

  // --- Reset ---
  const resetAnnotations = useCallback(() => {
    setAnnotations([]); setShowAnnotationPanel(false); setSelectionToolbar(null);
  }, []);

  return {
    annotations, setAnnotations,
    showAnnotationPanel, selectionToolbar, selectionToolbarRef,
    handleTextSelected, handleHighlight, handleNote, handleCopy,
    handleDeleteAnnotation,
    toggleAnnotations, handleAnnotationNavigate, handleAnnotationPanelClose,
    resetAnnotations, saveAnnotations,
  };
}
