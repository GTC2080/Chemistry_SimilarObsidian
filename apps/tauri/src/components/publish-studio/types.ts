import type { NoteInfo } from "../../types";

export interface PublishStudioProps {
  notes: NoteInfo[];
  initialContent: string;
  onSave: (content: string) => void | Promise<void>;
}

export interface PaperDocumentState {
  nodeIds: string[];
  template: string;
  cslPath: string;
  bibliographyPath: string;
}

export interface CompilerEnvironmentStatus {
  ready: boolean;
  pandocAvailable: boolean;
  latexEngineAvailable: boolean;
  message: string;
}

export interface CompilePayload {
  markdown: string;
  imagePaths: string[];
  template: string;
  cslPath: string | null;
  bibliographyPath: string | null;
}

export interface DragMeta {
  origin: "source" | "assembly";
  noteId: string;
}

export const ASSEMBLY_DROP_ID = "assembly:drop";
