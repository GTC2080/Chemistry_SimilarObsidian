/**
 * PDF 目录（outline）加载
 */

import { useCallback, useEffect, useRef, useState } from "react";
import type { PDFDocumentProxy } from "pdfjs-dist";
import type { OutlineEntry } from "../../../types/pdf";

export function usePdfOutline(docId: string | null, doc: PDFDocumentProxy | null) {
  const [showOutline, setShowOutline] = useState(false);
  const [outline, setOutline] = useState<OutlineEntry[]>([]);
  const loadedRef = useRef(false);

  useEffect(() => {
    if (!showOutline || !docId || loadedRef.current || !doc) return;
    let cancelled = false;
    loadedRef.current = true;

    void (async () => {
      try {
        const raw = await doc.getOutline();
        if (cancelled || !raw) { if (!cancelled) setOutline([]); return; }

        async function convert(items: NonNullable<typeof raw>): Promise<OutlineEntry[]> {
          const result: OutlineEntry[] = [];
          for (const item of items) {
            let page: number | null = null;
            if (item.dest) {
              try {
                const dest = typeof item.dest === "string" ? await doc!.getDestination(item.dest) : item.dest;
                if (dest) page = await doc!.getPageIndex(dest[0] as never);
              } catch { /* ignore */ }
            }
            result.push({ title: item.title, page, children: item.items ? await convert(item.items) : [] });
          }
          return result;
        }

        const entries = await convert(raw);
        if (!cancelled) setOutline(entries);
      } catch { if (!cancelled) setOutline([]); }
    })();
    return () => { cancelled = true; };
  }, [showOutline, docId, doc]);

  const toggleOutline = useCallback(() => setShowOutline((p) => !p), []);
  const handleOutlineClose = useCallback(() => setShowOutline(false), []);

  const resetOutline = useCallback(() => {
    setShowOutline(false); setOutline([]); loadedRef.current = false;
  }, []);

  return { showOutline, outline, toggleOutline, handleOutlineClose, resetOutline };
}
