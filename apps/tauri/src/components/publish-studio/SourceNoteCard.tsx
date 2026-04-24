import { useDraggable } from "@dnd-kit/core";
import type { CSSProperties } from "react";

import type { NoteInfo } from "../../types";
import type { DragMeta } from "./types";

export default function SourceNoteCard({ note, selected }: { note: NoteInfo; selected: boolean }) {
  const { attributes, listeners, setNodeRef, isDragging } = useDraggable({
    id: `source:${note.id}`,
    data: { origin: "source", noteId: note.id } satisfies DragMeta,
  });

  const style: CSSProperties = {
    opacity: isDragging ? 0.45 : selected ? 0.65 : 1,
  };

  return (
    <div
      ref={setNodeRef}
      {...listeners}
      {...attributes}
      style={style}
      className="bg-[#141414] border border-[#333] p-3 mb-2 rounded text-sm text-[#CCC] cursor-grab hover:border-[#555]"
      title={note.path}
    >
      <div className="truncate">{note.name}</div>
      <div className="text-[11px] text-[#777] mt-1 truncate">{note.id}</div>
    </div>
  );
}
