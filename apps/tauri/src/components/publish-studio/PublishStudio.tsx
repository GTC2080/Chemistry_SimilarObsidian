import {
  DndContext,
  PointerSensor,
  closestCenter,
  useDroppable,
  useSensor,
  useSensors,
} from "@dnd-kit/core";
import { SortableContext, verticalListSortingStrategy } from "@dnd-kit/sortable";

import PdfViewer from "../PdfViewer";
import { useT } from "../../i18n";
import type { PublishStudioProps } from "./types";
import { ASSEMBLY_DROP_ID } from "./types";
import { usePublishStudio } from "./usePublishStudio";
import SourceNoteCard from "./SourceNoteCard";
import AssemblyNoteCard from "./AssemblyNoteCard";

export default function PublishStudio({ notes, initialContent, onSave }: PublishStudioProps) {
  const t = useT();
  const {
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
  } = usePublishStudio(notes, initialContent, onSave);

  const sensors = useSensors(
    useSensor(PointerSensor, { activationConstraint: { distance: 4 } })
  );
  const { setNodeRef: setDropRef, isOver: isDropOver } = useDroppable({ id: ASSEMBLY_DROP_ID });

  return (
    <div className="flex h-screen w-full bg-[#050505] text-[#EDEDED] overflow-hidden">
      <aside className="w-1/3 border-r border-[#222] flex flex-col">
        {/* Toolbar */}
        <div className="p-4 border-b border-[#1C1C1C]">
          <button
            type="button"
            onClick={handleBuildPdf}
            disabled={compiling || statusLoading}
            className={`bg-[#EDEDED] text-[#050505] px-4 py-2 text-sm font-bold ${compiling ? "animate-pulse" : ""} disabled:opacity-50`}
          >
            {compiling ? "[ COMPILING... ]" : "Build PDF"}
          </button>
          <div className="mt-3 grid grid-cols-1 gap-2 text-xs text-[#A8A8A8]">
            <label className="flex items-center gap-2">
              <span className="w-20 text-[#8A8A8A]">Template</span>
              <select
                value={paperState.template}
                onChange={e => setPaperState(prev => ({ ...prev, template: e.target.value }))}
                className="flex-1 bg-[#0F0F0F] border border-[#2A2A2A] px-2 py-1 text-[#DDD] outline-none"
              >
                <option value="standard-thesis">standard-thesis</option>
                <option value="acs">acs</option>
                <option value="nature">nature</option>
              </select>
            </label>
            <label className="flex items-center gap-2">
              <span className="w-20 text-[#8A8A8A]">CSL</span>
              <input
                value={paperState.cslPath}
                onChange={e => setPaperState(prev => ({ ...prev, cslPath: e.target.value }))}
                className="flex-1 bg-[#0F0F0F] border border-[#2A2A2A] px-2 py-1 text-[#DDD] outline-none"
                placeholder="path/to/style.csl"
              />
            </label>
            <label className="flex items-center gap-2">
              <span className="w-20 text-[#8A8A8A]">BibTeX</span>
              <input
                value={paperState.bibliographyPath}
                onChange={e => setPaperState(prev => ({ ...prev, bibliographyPath: e.target.value }))}
                className="flex-1 bg-[#0F0F0F] border border-[#2A2A2A] px-2 py-1 text-[#DDD] outline-none"
                placeholder="path/to/references.bib"
              />
            </label>
          </div>
          {compilerStatus && !compilerStatus.ready && (
            <div className="mt-3 text-xs text-[#FF9B9B] whitespace-pre-wrap border border-[#4A2222] bg-[#120A0A] p-2">
              {compilerStatus.message}
            </div>
          )}
          {compileError && (
            <div className="mt-3 text-xs text-[#FF9B9B] whitespace-pre-wrap border border-[#4A2222] bg-[#120A0A] p-2 max-h-36 overflow-auto">
              {compileError}
            </div>
          )}
        </div>

        {/* Drag & Drop panels */}
        <DndContext sensors={sensors} collisionDetection={closestCenter} onDragEnd={handleDragEnd}>
          <div className="flex-1 min-h-0 p-4 grid grid-cols-1 gap-4">
            <section className="min-h-0 overflow-auto">
              <div className="text-xs uppercase tracking-[0.2em] text-[#6E6E6E] mb-2">Source Nodes</div>
              <div className="pr-1">
                {markdownNotes.map(note => (
                  <SourceNoteCard key={note.id} note={note} selected={assemblyIdSet.has(note.id)} />
                ))}
                {markdownNotes.length === 0 && (
                  <div className="text-xs text-[#666] border border-dashed border-[#2B2B2B] rounded p-3">
                    {t("publish.noNotes")}
                  </div>
                )}
              </div>
            </section>

            <section ref={setDropRef} className={`min-h-0 overflow-auto rounded ${isDropOver ? "bg-[#101010]" : ""}`}>
              <div className="text-xs uppercase tracking-[0.2em] text-[#6E6E6E] mb-2">Assembly Line</div>
              <SortableContext items={assemblyNotes.map(note => note.id)} strategy={verticalListSortingStrategy}>
                <div className="pr-1">
                  {assemblyNotes.map(note => (
                    <AssemblyNoteCard key={note.id} note={note} onRemove={handleRemoveAssemblyNote} />
                  ))}
                </div>
              </SortableContext>
              {assemblyNotes.length === 0 && (
                <div className="text-xs text-[#666] border border-dashed border-[#2B2B2B] rounded p-3">
                  {t("publish.dragToAssemble")}
                </div>
              )}
            </section>
          </div>
        </DndContext>
      </aside>

      <section className="flex-1 bg-[#111] flex items-center justify-center p-8">
        <div className="w-full h-full max-w-[1100px]">
          <PdfViewer
            pdfBytes={pdfBytes}
            placeholder={compiling ? "[ COMPILING... ]" : "PDF PREVIEW"}
          />
        </div>
      </section>
    </div>
  );
}
