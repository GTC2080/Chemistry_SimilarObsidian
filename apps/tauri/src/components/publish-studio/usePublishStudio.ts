import { invoke } from "@tauri-apps/api/core";
import { arrayMove } from "@dnd-kit/sortable";
import { useEffect, useMemo, useRef, useState } from "react";
import type { DragEndEvent } from "@dnd-kit/core";

import type { NoteInfo } from "../../types";
import { getFileCategory } from "../../types";
import type {
  CompilePayload,
  CompilerEnvironmentStatus,
  DragMeta,
  PaperDocumentState,
} from "./types";
import { ASSEMBLY_DROP_ID } from "./types";
import { parsePaperState, serializePaperState } from "./paperState";
import { collectImagePaths, preprocessMarkdown } from "./utils";

export function usePublishStudio(
  notes: NoteInfo[],
  initialContent: string,
  onSave: (content: string) => void | Promise<void>,
) {
  const [paperState, setPaperState] = useState<PaperDocumentState>(() => parsePaperState(initialContent));
  const [compilerStatus, setCompilerStatus] = useState<CompilerEnvironmentStatus | null>(null);
  const [statusLoading, setStatusLoading] = useState(true);
  const [compiling, setCompiling] = useState(false);
  const [compileError, setCompileError] = useState("");
  const [pdfBytes, setPdfBytes] = useState<Uint8Array | null>(null);
  const saveSnapshotRef = useRef(serializePaperState(parsePaperState(initialContent)));

  const markdownNotes = useMemo(
    () => notes.filter(note => getFileCategory(note.file_extension) === "markdown"),
    [notes]
  );

  const markdownById = useMemo(
    () => new Map(markdownNotes.map(note => [note.id, note])),
    [markdownNotes]
  );

  const assemblyNotes = useMemo(
    () => paperState.nodeIds
      .map(noteId => markdownById.get(noteId))
      .filter((note): note is NoteInfo => !!note),
    [paperState.nodeIds, markdownById]
  );

  const assemblyIdSet = useMemo(
    () => new Set(paperState.nodeIds),
    [paperState.nodeIds]
  );

  // Sync state when initialContent changes (e.g. switching notes)
  useEffect(() => {
    const parsed = parsePaperState(initialContent);
    setPaperState(parsed);
    saveSnapshotRef.current = serializePaperState(parsed);
    setCompileError("");
    setPdfBytes(null);
  }, [initialContent]);

  // Prune deleted notes from assembly
  useEffect(() => {
    setPaperState(prev => {
      const normalized = prev.nodeIds.filter(noteId => markdownById.has(noteId));
      if (normalized.length === prev.nodeIds.length) return prev;
      return { ...prev, nodeIds: normalized };
    });
  }, [markdownById]);

  // Check compiler environment on mount
  useEffect(() => {
    let cancelled = false;
    setStatusLoading(true);

    invoke<CompilerEnvironmentStatus>("get_compiler_status")
      .then(status => {
        if (!cancelled) setCompilerStatus(status);
      })
      .catch(error => {
        if (!cancelled) {
          setCompilerStatus({
            ready: false,
            pandocAvailable: false,
            latexEngineAvailable: false,
            message: error instanceof Error ? error.message : String(error),
          });
        }
      })
      .finally(() => {
        if (!cancelled) setStatusLoading(false);
      });

    return () => { cancelled = true; };
  }, []);

  // Auto-save with debounce
  useEffect(() => {
    const serialized = serializePaperState(paperState);
    if (serialized === saveSnapshotRef.current) return;

    const timer = window.setTimeout(() => {
      void Promise.resolve(onSave(serialized))
        .then(() => { saveSnapshotRef.current = serialized; })
        .catch(() => undefined);
    }, 350);

    return () => { window.clearTimeout(timer); };
  }, [paperState, onSave]);

  const handleDragEnd = (event: DragEndEvent) => {
    const activeMeta = event.active.data.current as Partial<DragMeta> | undefined;
    const over = event.over;
    if (!activeMeta?.noteId || !over) return;
    const activeNoteId = activeMeta.noteId;

    const overId = String(over.id);
    const overMeta = over.data.current as Partial<DragMeta> | undefined;

    setPaperState(prev => {
      const ids = [...prev.nodeIds];

      if (activeMeta.origin === "source") {
        if (ids.includes(activeNoteId)) return prev;

        let insertIndex = ids.length;
        if (overId !== ASSEMBLY_DROP_ID) {
          const candidateId = overMeta?.noteId ?? overId;
          const idx = ids.indexOf(candidateId);
          if (idx >= 0) insertIndex = idx;
        }

        ids.splice(insertIndex, 0, activeNoteId);
        return { ...prev, nodeIds: ids };
      }

      if (activeMeta.origin === "assembly") {
        const oldIndex = ids.indexOf(activeNoteId);
        if (oldIndex < 0) return prev;

        let newIndex = oldIndex;
        if (overId === ASSEMBLY_DROP_ID) {
          newIndex = ids.length - 1;
        } else {
          const candidateId = overMeta?.noteId ?? overId;
          const idx = ids.indexOf(candidateId);
          if (idx >= 0) newIndex = idx;
        }

        if (newIndex === oldIndex) return prev;
        return { ...prev, nodeIds: arrayMove(ids, oldIndex, newIndex) };
      }

      return prev;
    });
  };

  const handleRemoveAssemblyNote = (noteId: string) => {
    setPaperState(prev => ({
      ...prev,
      nodeIds: prev.nodeIds.filter(id => id !== noteId),
    }));
  };

  const handleBuildPdf = async () => {
    setCompileError("");

    if (statusLoading) {
      setCompileError("正在检测编译环境，请稍后重试。");
      return;
    }
    if (!compilerStatus?.ready) {
      setCompileError(compilerStatus?.message || "未检测到 Pandoc/XeLaTeX 环境。");
      return;
    }
    if (assemblyNotes.length === 0) {
      setCompileError("请先拖入至少一个 Markdown 节点。");
      return;
    }

    setCompiling(true);
    try {
      const chunks = await Promise.all(
        assemblyNotes.map(async note => {
          const raw = await invoke<string>("read_note", { filePath: note.path });
          const cleaned = preprocessMarkdown(raw);
          const imagePaths = collectImagePaths(cleaned, note.path);
          return { title: note.name, markdown: cleaned, imagePaths };
        })
      );

      const fullMarkdown = chunks
        .map(chunk => `# ${chunk.title}\n\n${chunk.markdown}`)
        .join("\n\n");

      const uniqueImagePaths = Array.from(
        new Set(chunks.flatMap(chunk => chunk.imagePaths))
      );

      const payload: CompilePayload = {
        markdown: fullMarkdown,
        imagePaths: uniqueImagePaths,
        template: paperState.template.trim() || "standard-thesis",
        cslPath: paperState.cslPath.trim() || null,
        bibliographyPath: paperState.bibliographyPath.trim() || null,
      };

      const pdfArray = await invoke<number[]>("compile_to_pdf", { payload });
      setPdfBytes(Uint8Array.from(pdfArray));
    } catch (error) {
      setCompileError(error instanceof Error ? error.message : String(error));
    } finally {
      setCompiling(false);
    }
  };

  return {
    paperState,
    setPaperState,
    compilerStatus,
    statusLoading,
    compiling,
    compileError,
    pdfBytes,
    markdownNotes,
    assemblyNotes,
    assemblyIdSet,
    handleDragEnd,
    handleRemoveAssemblyNote,
    handleBuildPdf,
  };
}
