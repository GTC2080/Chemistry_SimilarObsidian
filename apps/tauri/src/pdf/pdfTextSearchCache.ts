import type { NormRect, PageTextData, SearchMatch, WordInfo } from "../types/pdf";

interface PdfTextContent {
  items: unknown[];
}

interface PdfViewport {
  width: number;
  height: number;
}

export interface PdfTextPage {
  getTextContent: () => Promise<PdfTextContent>;
  getViewport: (options: { scale: number }) => PdfViewport;
}

export interface PdfTextDocument {
  numPages: number;
  getPage: (pageNumber: number) => Promise<PdfTextPage>;
}

export interface PdfTextSearchCache {
  getPageText: (pageIndex: number) => Promise<PageTextData>;
  search: (query: string) => Promise<SearchMatch[]>;
}

function readTextItem(item: unknown): {
  text: string;
  transform: number[];
  width: number;
  height: number;
} | null {
  if (typeof item !== "object" || item === null || !("str" in item)) {
    return null;
  }
  const candidate = item as {
    str?: unknown;
    transform?: unknown;
    width?: unknown;
    height?: unknown;
  };
  if (typeof candidate.str !== "string" || candidate.str.length === 0) {
    return null;
  }
  if (
    !Array.isArray(candidate.transform) ||
    candidate.transform.length < 6 ||
    candidate.transform.some((value) => typeof value !== "number")
  ) {
    return null;
  }
  return {
    text: candidate.str,
    transform: candidate.transform,
    width: typeof candidate.width === "number" ? candidate.width : 0,
    height: typeof candidate.height === "number"
      ? candidate.height
      : Math.abs(candidate.transform[3] ?? 0),
  };
}

async function extractPageText(doc: PdfTextDocument, pageIndex: number): Promise<PageTextData> {
  const page = await doc.getPage(pageIndex + 1);
  const textContent = await page.getTextContent();
  const viewport = page.getViewport({ scale: 1.0 });

  const words: WordInfo[] = [];
  const fullTextParts: string[] = [];
  let charIndex = 0;

  for (const item of textContent.items) {
    const textItem = readTextItem(item);
    if (!textItem) continue;

    const tx = textItem.transform[4];
    const ty = textItem.transform[5];
    const rect: NormRect = {
      x: tx / viewport.width,
      y: 1 - (ty + textItem.height) / viewport.height,
      w: textItem.width / viewport.width,
      h: textItem.height / viewport.height,
    };

    words.push({ word: textItem.text, char_index: charIndex, rect });
    fullTextParts.push(textItem.text);
    charIndex += textItem.text.length + 1;
  }

  return { text: fullTextParts.join(" "), words };
}

export function createPdfTextSearchCache(doc: PdfTextDocument): PdfTextSearchCache {
  const pageTextCache = new Map<number, Promise<PageTextData>>();
  let searchGeneration = 0;

  const getPageText = (pageIndex: number) => {
    let cached = pageTextCache.get(pageIndex);
    if (!cached) {
      cached = extractPageText(doc, pageIndex);
      pageTextCache.set(pageIndex, cached);
    }
    return cached;
  };

  const search = async (query: string): Promise<SearchMatch[]> => {
    const trimmed = query.trim();
    if (!trimmed) return [];

    const generation = ++searchGeneration;
    const lowerQuery = trimmed.toLowerCase();
    const matches: SearchMatch[] = [];

    for (let pageIndex = 0; pageIndex < doc.numPages; pageIndex += 1) {
      const pageText = await getPageText(pageIndex);
      if (generation !== searchGeneration) return [];

      for (const word of pageText.words) {
        if (word.word.toLowerCase().includes(lowerQuery)) {
          matches.push({ page: pageIndex, rects: [word.rect] });
        }
      }
    }

    return generation === searchGeneration ? matches : [];
  };

  return { getPageText, search };
}
