/** 笔记元数据，与后端 Rust NoteInfo 结构体一一对应 */
export interface NoteInfo {
  id: string;
  name: string;
  path: string;
  created_at: number;
  updated_at: number;
  file_extension: string;
}

/** 文件类型分类 */
export type FileCategory =
  | "markdown"
  | "image"
  | "pdf"
  | "chem"
  | "paper"
  | "spectroscopy"
  | "molecular"
  | "code";

const IMAGE_EXTENSIONS = new Set(["png", "jpg", "jpeg", "gif", "svg", "webp", "bmp", "ico"]);
const PDF_EXTENSIONS = new Set(["pdf"]);
const MARKDOWN_EXTENSIONS = new Set(["md"]);
const CHEM_EXTENSIONS = new Set(["mol", "chemdraw"]);
const PAPER_EXTENSIONS = new Set(["paper"]);
const SPECTROSCOPY_EXTENSIONS = new Set(["csv", "jdx"]);
const MOLECULAR_EXTENSIONS = new Set(["pdb", "xyz", "cif"]);

export function getFileCategory(ext: string): FileCategory {
  const lower = ext.toLowerCase();
  if (MARKDOWN_EXTENSIONS.has(lower)) return "markdown";
  if (IMAGE_EXTENSIONS.has(lower)) return "image";
  if (PDF_EXTENSIONS.has(lower)) return "pdf";
  if (CHEM_EXTENSIONS.has(lower)) return "chem";
  if (PAPER_EXTENSIONS.has(lower)) return "paper";
  if (SPECTROSCOPY_EXTENSIONS.has(lower)) return "spectroscopy";
  if (MOLECULAR_EXTENSIONS.has(lower)) return "molecular";
  return "code";
}

/** 标签聚合信息 */
export interface TagInfo {
  name: string;
  count: number;
}

/** 图谱节点 */
export interface GraphNode {
  id: string;
  name: string;
  ghost: boolean;
}

/** 图谱连线 */
export interface GraphLink {
  source: string;
  target: string;
  /** 连线类型："link" = wikilink, "tag" = 标签共现, "folder" = 同文件夹 */
  kind: string;
}

/** 全局关系图谱数据 */
export interface GraphData {
  nodes: GraphNode[];
  links: GraphLink[];
}

export interface FileTreeNode {
  name: string;
  fullName: string;
  relativePath: string;
  isFolder: boolean;
  note?: NoteInfo;
  children: FileTreeNode[];
  fileCount: number;
}

export interface SpectrumSeries {
  y: number[];
  label: string;
}

export interface SpectrumData {
  x: number[];
  series: SpectrumSeries[];
  x_label: string;
  title: string;
  is_nmr: boolean;
}

export interface MolecularPreviewMeta {
  atom_count: number;
  preview_atom_count: number;
  truncated: boolean;
}
