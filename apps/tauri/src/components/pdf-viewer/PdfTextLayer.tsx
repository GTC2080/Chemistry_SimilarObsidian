import { memo, useCallback, useEffect, useRef, useState } from "react";
import type { WordInfo, TextRange } from "../../types/pdf";
import { usePdfRenderer } from "../../hooks/usePdfRenderer";

export interface TextSelectionInfo {
  pageIndex: number;
  selectedText: string;
  textRanges: TextRange[];
  clientX: number;
  clientY: number;
}

interface PdfTextLayerProps {
  pageIndex: number;
  isVisible: boolean;
  onTextSelected?: (info: TextSelectionInfo) => void;
}

const pageTextCache = new Map<string, WordInfo[]>();
const TEXT_FETCH_DELAY_MS = 120;

const PdfTextLayer = memo(function PdfTextLayer({
  pageIndex,
  isVisible,
  onTextSelected,
}: PdfTextLayerProps) {
  const { docId, getPageText } = usePdfRenderer();
  const [words, setWords] = useState<WordInfo[]>([]);
  const requestKeyRef = useRef(0);
  const layerRef = useRef<HTMLDivElement>(null);

  useEffect(() => {
    if (!isVisible || !docId) {
      return;
    }

    const cacheKey = `${docId}:${pageIndex}`;
    if (pageTextCache.has(cacheKey)) {
      setWords(pageTextCache.get(cacheKey) ?? []);
      return;
    }
    setWords([]);

    const key = ++requestKeyRef.current;
    const run = () => {
      getPageText(pageIndex)
        .then((data) => {
          if (requestKeyRef.current === key) {
            pageTextCache.set(cacheKey, data.words);
            setWords(data.words);
          }
        })
        .catch(() => {
          // Text extraction is optional — silently ignore failures
        });
    };

    let timeoutId: ReturnType<typeof setTimeout> | null = null;
    let idleId: number | null = null;

    if (typeof window !== "undefined" && "requestIdleCallback" in window) {
      idleId = window.requestIdleCallback(run, { timeout: TEXT_FETCH_DELAY_MS });
    } else {
      timeoutId = setTimeout(run, TEXT_FETCH_DELAY_MS);
    }

    return () => {
      if (idleId !== null && typeof window !== "undefined" && "cancelIdleCallback" in window) {
        window.cancelIdleCallback(idleId);
      }
      if (timeoutId !== null) {
        clearTimeout(timeoutId);
      }
    };
  }, [isVisible, docId, pageIndex, getPageText]);

  const handleMouseUp = useCallback(
    (e: React.MouseEvent) => {
      if (!onTextSelected || !layerRef.current) return;

      const selection = window.getSelection();
      if (!selection || selection.isCollapsed || !selection.rangeCount) return;

      const text = selection.toString().trim();
      if (!text) return;

      const range = selection.getRangeAt(0);
      const clientRects = range.getClientRects();
      if (clientRects.length === 0) return;

      const layerEl = layerRef.current;
      const layerRect = layerEl.getBoundingClientRect();

      // Convert client rects to normalized coordinates relative to the text layer
      const textRanges: TextRange[] = [];
      const rects = [];
      for (let i = 0; i < clientRects.length; i++) {
        const cr = clientRects[i];
        rects.push({
          x: (cr.left - layerRect.left) / layerRect.width,
          y: (cr.top - layerRect.top) / layerRect.height,
          w: cr.width / layerRect.width,
          h: cr.height / layerRect.height,
        });
      }

      if (rects.length > 0) {
        textRanges.push({
          startOffset: 0,
          endOffset: text.length,
          rects,
        });
      }

      onTextSelected({
        pageIndex,
        selectedText: text,
        textRanges,
        clientX: e.clientX,
        clientY: e.clientY,
      });
    },
    [onTextSelected, pageIndex],
  );

  if (words.length === 0) {
    return null;
  }

  return (
    <div className="pdf-text-layer" ref={layerRef} onMouseUp={handleMouseUp}>
      {words.map((w, i) => (
        <span
          key={i}
          style={{
            left: `${w.rect.x * 100}%`,
            top: `${w.rect.y * 100}%`,
            width: `${w.rect.w * 100}%`,
            height: `${w.rect.h * 100}%`,
            fontSize: `${w.rect.h * 100}%`,
          }}
        >
          {w.word}
        </span>
      ))}
    </div>
  );
});

export default PdfTextLayer;
