// --- Rust IPC response types ---

/** Matches Rust `PageDimension` (width/height in PDF points) */
export interface PageDimension {
  width: number;
  height: number;
}

export interface PdfMetadata {
  doc_id: string;
  page_count: number;
  page_dimensions: PageDimension[];
  outline: OutlineEntry[];
}

export interface OutlineEntry {
  title: string;
  /** 0-based page index; null when the target page could not be resolved */
  page: number | null;
  children: OutlineEntry[];
}

export interface RenderResult {
  file_path: string;
  data_url?: string | null;
  width: number;
  height: number;
}

export interface PageTextData {
  text: string;
  words: WordInfo[];
}

export interface WordInfo {
  word: string;
  char_index: number;
  rect: NormRect;
}

export interface NormRect {
  x: number;
  y: number;
  w: number;
  h: number;
}

export interface SearchMatch {
  page: number;
  rects: NormRect[];
}

// --- Annotation types ---
export type AnnotationColor = "yellow" | "red" | "green" | "blue" | "purple";
export type AnnotationType = "highlight" | "note" | "area" | "ink";

export interface InkPoint {
  x: number;
  y: number;
  pressure: number;
}

export interface InkStroke {
  points: InkPoint[];
  strokeWidth: number;
}

export interface TextRange {
  startOffset: number;
  endOffset: number;
  rects: NormRect[];
}

export interface PdfAnnotation {
  id: string;
  pageNumber: number;
  type: AnnotationType;
  color: AnnotationColor;
  textRanges?: TextRange[];
  area?: NormRect;
  content?: string;
  selectedText?: string;
  inkStrokes?: InkStroke[];
  createdAt: string;
  updatedAt: string;
}

// --- Viewer state types ---
export type ZoomPreset = "fit-width" | "fit-page" | number;

export interface PdfViewerState {
  docId: string | null;
  metadata: PdfMetadata | null;
  currentPage: number;
  zoom: number;
  searchQuery: string;
  searchResults: SearchMatch[];
  searchIndex: number;
  annotations: PdfAnnotation[];
  showSearch: boolean;
  showOutline: boolean;
  showAnnotationPanel: boolean;
}

export const HIGHLIGHT_COLORS: Record<AnnotationColor, string> = {
  yellow: "rgba(255, 208, 0, 0.35)",
  red: "rgba(255, 69, 58, 0.35)",
  green: "rgba(50, 215, 75, 0.35)",
  blue: "rgba(10, 132, 255, 0.35)",
  purple: "rgba(191, 90, 242, 0.35)",
};
