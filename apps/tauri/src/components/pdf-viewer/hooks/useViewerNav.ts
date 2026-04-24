/**
 * 页面导航、缩放、滚动位置记忆、IntersectionObserver
 */

import { useCallback, useEffect, useRef, useState } from "react";
import type { PdfMetadata } from "../../../types/pdf";

const DEFAULT_ZOOM = 1;
const MIN_ZOOM = 0.25;
const MAX_ZOOM = 4;
const TOOLBAR_HIDE_DELAY = 3000;
const SCROLL_SAVE_DEBOUNCE = 1000;

export const PAGE_BASE_WIDTH = 612;
export const PAGE_BASE_HEIGHT = 792;

function clampZoom(z: number): number {
  return Math.min(MAX_ZOOM, Math.max(MIN_ZOOM, z));
}

export type ViewerStatus = "loading" | "ready" | "error";

export function useViewerNav(noteId: string, metadata: PdfMetadata | null) {
  const [status, setStatus] = useState<ViewerStatus>("loading");
  const [errorMessage, setErrorMessage] = useState("");
  const [currentPage, setCurrentPage] = useState(1);
  const [zoom, setZoom] = useState(DEFAULT_ZOOM);
  const [toolbarVisible, setToolbarVisible] = useState(true);
  const [visiblePages, setVisiblePages] = useState<Set<number>>(new Set());

  const hideTimerRef = useRef<ReturnType<typeof setTimeout> | null>(null);
  const containerRef = useRef<HTMLDivElement>(null);
  const scrollRef = useRef<HTMLDivElement>(null);
  const pageRefs = useRef<Map<number, HTMLDivElement>>(new Map());
  const observerRef = useRef<IntersectionObserver | null>(null);
  const scrollSaveTimerRef = useRef<ReturnType<typeof setTimeout> | null>(null);
  const prevZoomRef = useRef(DEFAULT_ZOOM);
  const restoredPositionRef = useRef(false);

  // --- Toolbar auto-hide ---
  const resetToolbarTimer = useCallback(() => {
    setToolbarVisible(true);
    if (hideTimerRef.current) clearTimeout(hideTimerRef.current);
    hideTimerRef.current = setTimeout(() => setToolbarVisible(false), TOOLBAR_HIDE_DELAY);
  }, []);

  useEffect(() => {
    return () => {
      if (hideTimerRef.current) clearTimeout(hideTimerRef.current);
      if (scrollSaveTimerRef.current) clearTimeout(scrollSaveTimerRef.current);
    };
  }, []);

  useEffect(() => { resetToolbarTimer(); }, [resetToolbarTimer]);

  const handleMouseMove = useCallback(() => { resetToolbarTimer(); }, [resetToolbarTimer]);

  // --- IntersectionObserver ---
  useEffect(() => {
    if (!scrollRef.current) return;
    observerRef.current = new IntersectionObserver(
      (entries) => {
        setVisiblePages((prev) => {
          const next = new Set(prev);
          for (const entry of entries) {
            const idx = Number((entry.target as HTMLElement).dataset.pageIndex);
            if (entry.isIntersecting) next.add(idx); else next.delete(idx);
          }
          if (next.size === prev.size && [...next].every((v) => prev.has(v))) return prev;
          return next;
        });
      },
      { root: scrollRef.current, rootMargin: "300px 0px", threshold: 0 },
    );
    return () => { observerRef.current?.disconnect(); observerRef.current = null; };
  }, [status]);

  useEffect(() => {
    if (visiblePages.size === 0) return;
    const sorted = [...visiblePages].sort((a, b) => a - b);
    setCurrentPage(sorted[0] + 1);
  }, [visiblePages]);

  const setPageRef = useCallback((pageIndex: number, el: HTMLDivElement | null) => {
    const observer = observerRef.current;
    const prev = pageRefs.current.get(pageIndex);
    if (prev && observer) observer.unobserve(prev);
    if (el) { pageRefs.current.set(pageIndex, el); if (observer) observer.observe(el); }
    else pageRefs.current.delete(pageIndex);
  }, []);

  // --- Position memory ---
  const positionKey = `pdf-position-${noteId}`;

  useEffect(() => {
    if (status !== "ready" || restoredPositionRef.current) return;
    restoredPositionRef.current = true;
    try {
      const saved = localStorage.getItem(positionKey);
      if (saved) {
        const { page, zoom: sz } = JSON.parse(saved) as { page: number; zoom: number };
        if (typeof sz === "number") { setZoom(clampZoom(sz)); prevZoomRef.current = clampZoom(sz); }
        if (typeof page === "number" && page >= 1) {
          requestAnimationFrame(() => { pageRefs.current.get(page - 1)?.scrollIntoView({ behavior: "instant", block: "start" }); });
        }
      }
    } catch { /* ignore */ }
  }, [status, positionKey]);

  useEffect(() => {
    const el = scrollRef.current;
    if (!el || status !== "ready") return;
    const onScroll = () => {
      if (scrollSaveTimerRef.current) clearTimeout(scrollSaveTimerRef.current);
      scrollSaveTimerRef.current = setTimeout(() => {
        try { localStorage.setItem(positionKey, JSON.stringify({ page: currentPage, zoom })); } catch { /* full */ }
      }, SCROLL_SAVE_DEBOUNCE);
    };
    el.addEventListener("scroll", onScroll, { passive: true });
    return () => el.removeEventListener("scroll", onScroll);
  }, [status, positionKey, currentPage, zoom]);

  // --- Zoom scroll preservation ---
  useEffect(() => {
    const el = scrollRef.current;
    if (!el || prevZoomRef.current === zoom) return;
    const ratio = zoom / prevZoomRef.current;
    const mid = el.scrollTop + el.clientHeight / 2;
    el.scrollTop = mid * ratio - el.clientHeight / 2;
    prevZoomRef.current = zoom;
  }, [zoom]);

  useEffect(() => {
    const el = scrollRef.current;
    if (!el) return;
    const onWheel = (e: WheelEvent) => {
      if (!e.ctrlKey) return;
      e.preventDefault();
      const rect = el.getBoundingClientRect();
      const frac = (e.clientY - rect.top) / rect.height;
      const pre = el.scrollTop + frac * el.clientHeight;
      const delta = e.deltaY > 0 ? -0.1 : 0.1;
      setZoom((prev) => {
        const next = clampZoom(prev + delta);
        const r = next / prev;
        requestAnimationFrame(() => { el.scrollTop = pre * r - frac * el.clientHeight; });
        prevZoomRef.current = prev;
        requestAnimationFrame(() => { prevZoomRef.current = next; });
        return next;
      });
    };
    el.addEventListener("wheel", onWheel, { passive: false });
    return () => el.removeEventListener("wheel", onWheel);
  }, []);

  // --- Navigation ---
  const scrollToPage = useCallback((page: number) => {
    pageRefs.current.get(page - 1)?.scrollIntoView({ behavior: "smooth", block: "start" });
  }, []);

  const handlePageChange = useCallback((page: number) => {
    const count = metadata?.page_count ?? 0;
    const clamped = Math.min(Math.max(1, page), count);
    setCurrentPage(clamped);
    scrollToPage(clamped);
  }, [metadata, scrollToPage]);

  // --- Reset (called when opening new PDF) ---
  const resetNav = useCallback(() => {
    setStatus("loading"); setErrorMessage(""); setCurrentPage(1); setZoom(DEFAULT_ZOOM);
    setVisiblePages(new Set([0]));
    restoredPositionRef.current = false; prevZoomRef.current = DEFAULT_ZOOM;
  }, []);

  return {
    status, setStatus, errorMessage, setErrorMessage,
    currentPage, zoom, setZoom, visiblePages, toolbarVisible,
    containerRef, scrollRef, setPageRef,
    handleMouseMove, handlePageChange, resetToolbarTimer, resetNav,
  };
}
