import { useCallback, useEffect, useState } from "react";
import { invoke } from "@tauri-apps/api/core";
import type { DisciplineProfile } from "../components/settings/settingsTypes";
import type { MolecularPreviewMeta, NoteInfo } from "../types";
import { getFileCategory } from "../types";
import { perf } from "../utils/perf";
import { useBinaryPreview } from "./useBinaryPreview";
import { useNoteContentCache } from "../contexts/NoteContentCache";

interface UseActiveNoteContentOptions {
  activeNote: NoteInfo | null;
  activeDiscipline: DisciplineProfile;
}

interface MolecularPreviewResponse {
  preview_data: string;
  atom_count: number;
  preview_atom_count: number;
  truncated: boolean;
}

async function readMolecularPreview(filePath: string) {
  return invoke<MolecularPreviewResponse>("read_molecular_preview", {
    filePath,
    maxAtoms: 2000,
  });
}

export function useActiveNoteContent({
  activeNote,
  activeDiscipline,
}: UseActiveNoteContentOptions) {
  const [noteContent, setNoteContent] = useState("");
  const [liveContent, setLiveContent] = useState("");
  const [molecularPreview, setMolecularPreview] = useState<MolecularPreviewMeta | null>(null);
  const { binaryPreviewUrl, clearBinaryPreview, loadBinaryPreview } = useBinaryPreview();
  const { readNote: readNoteCached } = useNoteContentCache();

  const resetContent = useCallback(() => {
    clearBinaryPreview();
    setNoteContent("");
    setLiveContent("");
    setMolecularPreview(null);
  }, [clearBinaryPreview]);

  useEffect(() => {
    if (!activeNote) {
      resetContent();
      return;
    }

    let cancelled = false;

    const loadActiveNoteContent = async () => {
      const endNoteSwitch = perf.start(`note-switch:${activeNote.file_extension}`);
      try {
        const category = getFileCategory(activeNote.file_extension);
        clearBinaryPreview();
        setMolecularPreview(null);

        if (category === "pdf") {
          // PdfViewer handles its own loading via Rust — skip binary preview.
          setNoteContent("");
          try {
            const indexed = await invoke<string>("read_note_indexed_content", {
              noteId: activeNote.id,
            });
            if (!cancelled) {
              setLiveContent(indexed);
            }
          } catch {
            if (!cancelled) {
              setLiveContent("");
            }
          }
          endNoteSwitch();
          return;
        }

        if (category === "image") {
          setNoteContent("");
          if (!cancelled) {
            setLiveContent("");
          }
          await loadBinaryPreview(activeNote);
          endNoteSwitch();
          return;
        }

        if (category === "molecular" && activeDiscipline === "chemistry") {
          try {
            const preview = await readMolecularPreview(activeNote.path);
            if (cancelled) {
              return;
            }

            setNoteContent(preview.preview_data);
            setLiveContent(preview.preview_data);
            setMolecularPreview({
              atom_count: preview.atom_count,
              preview_atom_count: preview.preview_atom_count,
              truncated: preview.truncated,
            });
            endNoteSwitch();
            return;
          } catch {
            // Fall through to full text loading when preview mode is unavailable.
          }
        }

        const content = await readNoteCached(activeNote.path, activeNote.updated_at);
        if (cancelled) {
          return;
        }
        setNoteContent(content);
        setLiveContent(content);
        endNoteSwitch();
      } catch {
        if (!cancelled) {
          setNoteContent("");
          setLiveContent("");
          setMolecularPreview(null);
        }
      }
    };

    void loadActiveNoteContent();

    return () => {
      cancelled = true;
    };
  }, [activeDiscipline, activeNote, clearBinaryPreview, loadBinaryPreview, resetContent]);

  return {
    noteContent,
    setNoteContent,
    liveContent,
    setLiveContent,
    molecularPreview,
    binaryPreviewUrl,
    resetContent,
  };
}
