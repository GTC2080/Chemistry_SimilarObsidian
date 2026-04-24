import { createContext, useCallback, useContext, useEffect, useRef, useState } from "react";
import { invoke } from "@tauri-apps/api/core";
import * as pdfjsLib from "pdfjs-dist";
import type {
  PDFDocumentProxy,
} from "pdfjs-dist";
import type {
  PdfAnnotation,
  PdfMetadata,
  PageDimension,
  OutlineEntry,
  PageTextData,
  WordInfo,
  NormRect,
  SearchMatch,
} from "../types/pdf";

// ---------------------------------------------------------------------------
// pdf.js Worker 配置
// ---------------------------------------------------------------------------

pdfjsLib.GlobalWorkerOptions.workerSrc = new URL(
  "pdfjs-dist/build/pdf.worker.min.mjs",
  import.meta.url,
).toString();

// ---------------------------------------------------------------------------
// PdfDocContext — shared doc across PdfViewer and its children
// ---------------------------------------------------------------------------

export interface PdfDocHandle {
  doc: PDFDocumentProxy;
  filePath: string;
}

export const PdfDocContext = createContext<PdfDocHandle | null>(null);

// ---------------------------------------------------------------------------
// usePdfLifecycle — 用 pdf.js 替代 Rust open_pdf / close_pdf
// ---------------------------------------------------------------------------

export interface PdfLifecycle {
  docId: string | null;
  docIdRef: React.MutableRefObject<string | null>;
  metadata: PdfMetadata | null;
  docHandle: PdfDocHandle | null;
  openPdf: (filePath: string) => Promise<void>;
  closePdf: () => Promise<void>;
}

export function usePdfLifecycle(): PdfLifecycle {
  const [docId, setDocId] = useState<string | null>(null);
  const [metadata, setMetadata] = useState<PdfMetadata | null>(null);
  const [docHandle, setDocHandle] = useState<PdfDocHandle | null>(null);
  const docIdRef = useRef<string | null>(null);
  const docRef = useRef<PDFDocumentProxy | null>(null);

  useEffect(() => {
    docIdRef.current = docId;
  }, [docId]);

  const openPdf = useCallback(async (filePath: string) => {
    // 关闭旧文档
    if (docRef.current) {
      docRef.current.destroy();
      docRef.current = null;
    }

    // 通过专用命令读取 PDF 原始字节（IPC Response，零 JSON 序列化）
    const data = await invoke<ArrayBuffer>("read_pdf_file", { filePath });

    const loadingTask = pdfjsLib.getDocument({
      data,
      cMapUrl: "https://cdn.jsdelivr.net/npm/pdfjs-dist@5.5.207/cmaps/",
      cMapPacked: true,
    });

    const doc = await loadingTask.promise;
    docRef.current = doc;

    // 提取元数据
    const pageCount = doc.numPages;
    const id = simpleHash(filePath);

    // 只读首页尺寸（绝大多数 PDF 所有页尺寸一致）
    const firstPage = await doc.getPage(1); // pdf.js 页码 1-based
    const vp = firstPage.getViewport({ scale: 1.0 });
    const dim: PageDimension = { width: vp.width, height: vp.height };
    const pageDimensions = Array.from({ length: pageCount }, () => dim);

    // 提取目录（不阻塞首屏，失败则留空）
    let outline: OutlineEntry[] = [];
    try {
      const rawOutline = await doc.getOutline();
      if (rawOutline) {
        async function convert(
          items: NonNullable<Awaited<ReturnType<PDFDocumentProxy["getOutline"]>>>,
        ): Promise<OutlineEntry[]> {
          const result: OutlineEntry[] = [];
          for (const item of items) {
            let page: number | null = null;
            if (item.dest) {
              try {
                const dest =
                  typeof item.dest === "string"
                    ? await doc.getDestination(item.dest)
                    : item.dest;
                if (dest) {
                  page = await doc.getPageIndex(dest[0] as never);
                }
              } catch { /* ignore unresolvable dest */ }
            }
            const children = item.items ? await convert(item.items) : [];
            result.push({ title: item.title, page, children });
          }
          return result;
        }
        outline = await convert(rawOutline);
      }
    } catch { /* PDF has no outline */ }

    const meta: PdfMetadata = {
      doc_id: id,
      page_count: pageCount,
      page_dimensions: pageDimensions,
      outline,
    };

    setDocId(id);
    setMetadata(meta);
    setDocHandle({ doc, filePath });
  }, []);

  const closePdf = useCallback(async () => {
    if (docRef.current) {
      docRef.current.destroy();
      docRef.current = null;
    }
    setDocId(null);
    setMetadata(null);
    setDocHandle(null);
  }, []);

  useEffect(() => {
    return () => {
      if (docRef.current) {
        docRef.current.destroy();
        docRef.current = null;
      }
    };
  }, []);

  return { docId, docIdRef, metadata, docHandle, openPdf, closePdf };
}

// ---------------------------------------------------------------------------
// usePdfRenderer — 用 pdf.js 替代 Rust render / text / search
// ---------------------------------------------------------------------------

export interface PdfRenderer {
  docId: string | null;
  /** 将 PDF 页面渲染到指定 Canvas 上（零编码、零磁盘 I/O） */
  renderToCanvas: (
    pageIndex: number,
    scale: number,
    canvas: HTMLCanvasElement,
  ) => Promise<{ width: number; height: number }>;
  getPageText: (pageIndex: number) => Promise<PageTextData>;
  searchPdf: (query: string) => Promise<SearchMatch[]>;
  getOutline: () => Promise<OutlineEntry[]>;
}

export function usePdfRenderer(): PdfRenderer {
  const handle = useContext(PdfDocContext);
  const docId = handle ? simpleHash(handle.filePath) : null;

  const renderToCanvas = useCallback(
    async (
      pageIndex: number,
      scale: number,
      canvas: HTMLCanvasElement,
    ): Promise<{ width: number; height: number }> => {
      if (!handle) {
        throw new Error("usePdfRenderer: no PDF document is open");
      }

      const page = await handle.doc.getPage(pageIndex + 1); // pdf.js 1-based
      const viewport = page.getViewport({ scale });

      canvas.width = Math.floor(viewport.width);
      canvas.height = Math.floor(viewport.height);

      const ctx = canvas.getContext("2d");
      if (!ctx) throw new Error("无法获取 Canvas 2D 上下文");

      await page.render({ canvasContext: ctx, viewport }).promise;

      return { width: canvas.width, height: canvas.height };
    },
    [handle],
  );

  const getPageText = useCallback(
    async (pageIndex: number): Promise<PageTextData> => {
      if (!handle) {
        throw new Error("usePdfRenderer: no PDF document is open");
      }

      const page = await handle.doc.getPage(pageIndex + 1);
      const textContent = await page.getTextContent();
      const viewport = page.getViewport({ scale: 1.0 });

      const words: WordInfo[] = [];
      let charIndex = 0;
      const fullTextParts: string[] = [];

      for (const item of textContent.items) {
        if (!("str" in item)) continue;
        const textItem = item as { str: string; transform: number[]; width: number; height: number };
        const text = textItem.str;
        if (!text) continue;

        // pdf.js transform: [scaleX, skewY, skewX, scaleY, translateX, translateY]
        const tx = textItem.transform[4];
        const ty = textItem.transform[5];
        const w = textItem.width;
        const h = textItem.height || Math.abs(textItem.transform[3]);

        // 归一化到 0-1（相对于页面尺寸）
        const rect: NormRect = {
          x: tx / viewport.width,
          y: 1 - (ty + h) / viewport.height, // PDF 坐标系 y 轴向上
          w: w / viewport.width,
          h: h / viewport.height,
        };

        words.push({ word: text, char_index: charIndex, rect });
        fullTextParts.push(text);
        charIndex += text.length + 1; // +1 for implicit space
      }

      return { text: fullTextParts.join(" "), words };
    },
    [handle],
  );

  const searchPdf = useCallback(
    async (query: string): Promise<SearchMatch[]> => {
      if (!handle || !query.trim()) return [];

      const matches: SearchMatch[] = [];
      const lowerQuery = query.toLowerCase();

      for (let i = 1; i <= handle.doc.numPages; i++) {
        const page = await handle.doc.getPage(i);
        const textContent = await page.getTextContent();
        const viewport = page.getViewport({ scale: 1.0 });

        for (const item of textContent.items) {
          if (!("str" in item)) continue;
          const textItem = item as { str: string; transform: number[]; width: number; height: number };
          if (textItem.str.toLowerCase().includes(lowerQuery)) {
            const tx = textItem.transform[4];
            const ty = textItem.transform[5];
            const w = textItem.width;
            const h = textItem.height || Math.abs(textItem.transform[3]);

            matches.push({
              page: i - 1, // 0-based
              rects: [{
                x: tx / viewport.width,
                y: 1 - (ty + h) / viewport.height,
                w: w / viewport.width,
                h: h / viewport.height,
              }],
            });
          }
        }
      }

      return matches;
    },
    [handle],
  );

  const getOutline = useCallback(async (): Promise<OutlineEntry[]> => {
    if (!handle) return [];

    const outline = await handle.doc.getOutline();
    if (!outline) return [];

    async function convertOutline(
      items: Awaited<ReturnType<PDFDocumentProxy["getOutline"]>>,
    ): Promise<OutlineEntry[]> {
      if (!items) return [];
      const result: OutlineEntry[] = [];

      for (const item of items) {
        let page: number | null = null;
        if (item.dest) {
          try {
            const dest = typeof item.dest === "string"
              ? await handle!.doc.getDestination(item.dest)
              : item.dest;
            if (dest) {
              const pageIndex = await handle!.doc.getPageIndex(dest[0] as never);
              page = pageIndex; // 0-based
            }
          } catch {
            // Ignore unresolvable destinations
          }
        }
        const children = item.items ? await convertOutline(item.items) : [];
        result.push({ title: item.title, page, children });
      }
      return result;
    }

    return convertOutline(outline);
  }, [handle]);

  return { docId, renderToCanvas, getPageText, searchPdf, getOutline };
}

// ---------------------------------------------------------------------------
// usePdfAnnotations — 批注保持 Rust IPC（本地文件 I/O）
// ---------------------------------------------------------------------------

export interface PdfAnnotationsHook {
  loadAnnotations: (vaultPath: string, filePath: string) => Promise<PdfAnnotation[]>;
  saveAnnotations: (
    vaultPath: string,
    filePath: string,
    annotationsData: PdfAnnotation[],
  ) => Promise<void>;
}

export function usePdfAnnotations(): PdfAnnotationsHook {
  const loadAnnotations = useCallback(
    async (vaultPath: string, filePath: string): Promise<PdfAnnotation[]> => {
      return invoke<PdfAnnotation[]>("load_pdf_annotations", { vaultPath, filePath });
    },
    [],
  );

  const saveAnnotations = useCallback(
    async (
      vaultPath: string,
      filePath: string,
      annotationsData: PdfAnnotation[],
    ): Promise<void> => {
      return invoke("save_pdf_annotations", { vaultPath, filePath, annotationsData });
    },
    [],
  );

  return { loadAnnotations, saveAnnotations };
}

// ---------------------------------------------------------------------------
// 工具函数
// ---------------------------------------------------------------------------

function simpleHash(str: string): string {
  let hash = 2166136261;
  for (let i = 0; i < str.length; i++) {
    hash ^= str.charCodeAt(i);
    hash = Math.imul(hash, 16777619);
  }
  return (hash >>> 0).toString(16).padStart(8, "0");
}
