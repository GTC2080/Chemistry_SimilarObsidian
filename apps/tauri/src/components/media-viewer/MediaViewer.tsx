import { useEffect, useRef, useState } from "react";
import { convertFileSrc } from "@tauri-apps/api/core";
import type { NoteInfo } from "../../types";
import { useT } from "../../i18n";
import "./media-viewer.css";

interface MediaViewerProps {
  category: "image" | "pdf";
  note: NoteInfo;
  binaryPreviewUrl: string;
}

export default function MediaViewer({ category, note, binaryPreviewUrl }: MediaViewerProps) {
  const t = useT();
  const [imageZoom, setImageZoom] = useState(1);
  const [imageNaturalSize, setImageNaturalSize] = useState<{ width: number; height: number } | null>(null);
  const [imageViewportSize, setImageViewportSize] = useState({ width: 0, height: 0 });
  const [imagePanning, setImagePanning] = useState(false);
  const imageViewportRef = useRef<HTMLDivElement | null>(null);
  const imagePanRef = useRef({
    active: false,
    startX: 0,
    startY: 0,
    startScrollLeft: 0,
    startScrollTop: 0,
  });

  useEffect(() => {
    setImageZoom(1);
    setImageNaturalSize(null);
    const viewport = imageViewportRef.current;
    if (viewport) {
      viewport.scrollTo({ left: 0, top: 0, behavior: "auto" });
    }
  }, [note.id, category]);

  useEffect(() => {
    function handleWindowMouseMove(e: MouseEvent) {
      if (!imagePanRef.current.active) return;
      const viewport = imageViewportRef.current;
      if (!viewport) return;
      const dx = e.clientX - imagePanRef.current.startX;
      const dy = e.clientY - imagePanRef.current.startY;
      viewport.scrollLeft = imagePanRef.current.startScrollLeft - dx;
      viewport.scrollTop = imagePanRef.current.startScrollTop - dy;
    }

    function handleWindowMouseUp() {
      if (!imagePanRef.current.active) return;
      imagePanRef.current.active = false;
      setImagePanning(false);
    }

    window.addEventListener("mousemove", handleWindowMouseMove);
    window.addEventListener("mouseup", handleWindowMouseUp);
    return () => {
      window.removeEventListener("mousemove", handleWindowMouseMove);
      window.removeEventListener("mouseup", handleWindowMouseUp);
    };
  }, []);

  useEffect(() => {
    const viewport = imageViewportRef.current;
    if (!viewport) return;
    const updateSize = () => {
      setImageViewportSize({
        width: viewport.clientWidth,
        height: viewport.clientHeight,
      });
    };
    updateSize();
    const observer = new ResizeObserver(updateSize);
    observer.observe(viewport);
    return () => observer.disconnect();
  }, [note.id, category]);

  function clampZoom(value: number): number {
    return Math.min(5, Math.max(0.2, value));
  }

  if (category === "pdf") {
    return (
      <div className="flex-1 overflow-hidden p-3 media-viewer-pdf-surface">
        {binaryPreviewUrl ? (
          <object
            data={binaryPreviewUrl}
            type="application/pdf"
            className="w-full h-full rounded-md media-viewer-pdf-object"
          >
            <div className="h-full flex items-center justify-center">
              <button
                type="button"
                onClick={() => window.open(convertFileSrc(note.path), "_blank")}
                className="px-4 py-2 rounded-md border border-white/20 text-white/90 hover:bg-white/10"
              >
                {t("media.pdfFallback")}
              </button>
            </div>
          </object>
        ) : (
          <div className="h-full flex items-center justify-center text-white/70 text-[13px]">
            {t("media.loadingPdf")}
          </div>
        )}
      </div>
    );
  }

  const viewportInnerWidth = Math.max(0, imageViewportSize.width - 64);
  const viewportInnerHeight = Math.max(0, imageViewportSize.height - 64);
  const fitScale = imageNaturalSize && viewportInnerWidth > 0 && viewportInnerHeight > 0
    ? Math.min(
      viewportInnerWidth / imageNaturalSize.width,
      viewportInnerHeight / imageNaturalSize.height,
      1
    )
    : 1;
  const effectiveScale = fitScale * imageZoom;
  const renderedWidth = imageNaturalSize
    ? Math.max(1, Math.round(imageNaturalSize.width * effectiveScale))
    : 0;
  const renderedHeight = imageNaturalSize
    ? Math.max(1, Math.round(imageNaturalSize.height * effectiveScale))
    : 0;
  const canPan = renderedWidth > viewportInnerWidth || renderedHeight > viewportInnerHeight;

  return (
    <div
      ref={imageViewportRef}
      className="flex-1 overflow-auto relative media-viewer-surface"
      onWheel={e => {
        e.preventDefault();
        const step = e.deltaY > 0 ? -0.1 : 0.1;
        setImageZoom(prev => clampZoom(prev + step));
      }}
      onMouseDown={e => {
        if (!canPan || e.button !== 0) return;
        const viewport = imageViewportRef.current;
        if (!viewport) return;
        imagePanRef.current.active = true;
        imagePanRef.current.startX = e.clientX;
        imagePanRef.current.startY = e.clientY;
        imagePanRef.current.startScrollLeft = viewport.scrollLeft;
        imagePanRef.current.startScrollTop = viewport.scrollTop;
        setImagePanning(true);
      }}
    >
      <div className="absolute right-5 top-5 z-10 flex items-center gap-1 rounded-md px-1.5 py-1 media-viewer-toolbar">
        <button
          type="button"
          className="w-7 h-7 rounded text-white/90 hover:bg-white/10"
          onClick={() => setImageZoom(prev => clampZoom(prev - 0.1))}
          aria-label={t("media.zoomOut")}
        >
          -
        </button>
        <span className="text-[11px] px-1.5 text-white/80 tabular-nums">
          {Math.round(imageZoom * 100)}%
        </span>
        <button
          type="button"
          className="w-7 h-7 rounded text-white/90 hover:bg-white/10"
          onClick={() => setImageZoom(prev => clampZoom(prev + 0.1))}
          aria-label={t("media.zoomIn")}
        >
          +
        </button>
        <button
          type="button"
          className="h-7 px-2 rounded text-[11px] text-white/90 hover:bg-white/10"
          onClick={() => {
            setImageZoom(1);
            const viewport = imageViewportRef.current;
            if (viewport) {
              viewport.scrollTo({ left: 0, top: 0, behavior: "smooth" });
            }
          }}
        >
          {t("media.reset")}
        </button>
      </div>
      <div className="min-h-full min-w-full p-6 box-border flex items-center justify-center">
        {binaryPreviewUrl ? (
          <div
            style={{
              width: `${Math.max(renderedWidth, viewportInnerWidth)}px`,
              height: `${Math.max(renderedHeight, viewportInnerHeight)}px`,
            }}
            className="flex items-center justify-center"
          >
            <img
              src={binaryPreviewUrl}
              alt={note.name}
              draggable={false}
              className="media-viewer-image"
              onLoad={e => {
                const target = e.currentTarget;
                if (target.naturalWidth && target.naturalHeight) {
                  setImageNaturalSize({
                    width: target.naturalWidth,
                    height: target.naturalHeight,
                  });
                }
              }}
              style={{
                width: renderedWidth > 0 ? `${renderedWidth}px` : "auto",
                height: renderedHeight > 0 ? `${renderedHeight}px` : "auto",
                cursor: canPan ? (imagePanning ? "grabbing" : "grab") : "default",
              }}
            />
          </div>
        ) : (
          <div className="text-white/70 text-[13px] px-8 py-6">{t("media.loadingImage")}</div>
        )}
      </div>
    </div>
  );
}
