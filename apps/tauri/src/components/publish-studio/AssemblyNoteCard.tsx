import { useSortable } from "@dnd-kit/sortable";
import type { CSSProperties } from "react";

import type { NoteInfo } from "../../types";
import type { DragMeta } from "./types";
import { toTransformString } from "./utils";

export default function AssemblyNoteCard({
  note,
  onRemove,
}: {
  note: NoteInfo;
  onRemove: (noteId: string) => void;
}) {
  const { attributes, listeners, setNodeRef, transform, transition, isDragging } = useSortable({
    id: note.id,
    data: { origin: "assembly", noteId: note.id } satisfies DragMeta,
  });

  const style: CSSProperties = {
    transform: toTransformString(transform),
    transition,
    opacity: isDragging ? 0.4 : 1,
  };

  return (
    <div
      ref={setNodeRef}
      style={style}
      className="bg-[#141414] border border-[#333] p-3 mb-2 rounded text-sm text-[#CCC] hover:border-[#555]"
    >
      <div className="flex items-center gap-2">
        <button
          type="button"
          {...listeners}
          {...attributes}
          className="text-[#8A8A8A] hover:text-[#DDD] cursor-grab"
          title="拖动排序"
          aria-label="拖动排序"
        >
          ::
        </button>
        <div className="min-w-0 flex-1">
          <div className="truncate">{note.name}</div>
          <div className="text-[11px] text-[#777] mt-1 truncate">{note.id}</div>
        </div>
        <button
          type="button"
          onClick={() => onRemove(note.id)}
          className="text-[#8A8A8A] hover:text-[#EEE] text-xs"
          title="移除"
          aria-label="移除"
        >
          REMOVE
        </button>
      </div>
    </div>
  );
}
