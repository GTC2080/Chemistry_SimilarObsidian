import { useEffect, useMemo, useState } from "react";
import type { NoteRecord } from "../../lib/files-tree";
import {
  getPdfMetadata,
  listAttachments,
  queryNotePdfSourceRefs,
  queryPdfReferrers,
  type HostAttachmentRecord,
  type HostPdfMetadata,
  type HostPdfReferrer,
  type HostPdfSourceRef
} from "../../lib/host-shell";
import {
  ToolActionButton,
  ToolBadge,
  ToolBody,
  ToolContentHeader,
  ToolDevDetails,
  ToolDetailSection,
  ToolEmptyState,
  ToolErrorBanner,
  ToolListButton,
  ToolMetaGrid,
  ToolMetric,
  ToolReferenceCard,
  ToolSection,
  ToolWorkspaceShell,
  formatPresenceLabel
} from "./ToolingScaffold";

interface PdfWorkspaceProps {
  visible: boolean;
  activeNote: NoteRecord | null;
  onOpenNote: (relPath: string) => void;
}

function isPdfAttachment(item: HostAttachmentRecord) {
  return item.extension.toLowerCase() === ".pdf" || item.basename.toLowerCase().endsWith(".pdf");
}

export default function PdfWorkspace({
  visible,
  activeNote,
  onOpenNote
}: PdfWorkspaceProps) {
  const [pdfAttachments, setPdfAttachments] = useState<HostAttachmentRecord[]>([]);
  const [selectedRelPath, setSelectedRelPath] = useState<string | null>(null);
  const [metadata, setMetadata] = useState<HostPdfMetadata | null>(null);
  const [referrers, setReferrers] = useState<HostPdfReferrer[]>([]);
  const [noteRefs, setNoteRefs] = useState<HostPdfSourceRef[]>([]);
  const [loadingList, setLoadingList] = useState(false);
  const [loadingDetail, setLoadingDetail] = useState(false);
  const [error, setError] = useState<string | null>(null);
  const [filterText, setFilterText] = useState("");

  useEffect(() => {
    if (!visible) {
      return;
    }

    let cancelled = false;
    void (async () => {
      setLoadingList(true);
      const envelope = await listAttachments();
      if (cancelled) {
        return;
      }

      if (!envelope?.ok || !envelope.data) {
        setPdfAttachments([]);
        setError(envelope?.error?.message ?? "读取 PDF 附件目录失败。");
        setLoadingList(false);
        return;
      }

      const items = (envelope.data.items ?? []).filter(isPdfAttachment);
      setPdfAttachments(items);
      setError(null);
      setLoadingList(false);
      if (!selectedRelPath && items.length > 0) {
        setSelectedRelPath(items[0].relPath);
      }
    })();

    return () => {
      cancelled = true;
    };
  }, [visible]);

  useEffect(() => {
    if (!visible || !activeNote?.relPath) {
      setNoteRefs([]);
      return;
    }

    let cancelled = false;
    void (async () => {
      const envelope = await queryNotePdfSourceRefs(activeNote.relPath);
      if (!cancelled) {
        setNoteRefs(envelope?.ok && envelope.data ? envelope.data.items ?? [] : []);
      }
    })();

    return () => {
      cancelled = true;
    };
  }, [visible, activeNote?.relPath]);

  useEffect(() => {
    if (!visible || !selectedRelPath) {
      setMetadata(null);
      setReferrers([]);
      return;
    }

    let cancelled = false;
    void (async () => {
      setLoadingDetail(true);
      const [metadataEnvelope, referrerEnvelope] = await Promise.all([
        getPdfMetadata(selectedRelPath),
        queryPdfReferrers(selectedRelPath)
      ]);

      if (cancelled) {
        return;
      }

      if (!metadataEnvelope?.ok || !metadataEnvelope.data) {
        setMetadata(null);
        setReferrers([]);
        setError(metadataEnvelope?.error?.message ?? "读取 PDF 信息失败。");
        setLoadingDetail(false);
        return;
      }

      setMetadata(metadataEnvelope.data);
      setReferrers(referrerEnvelope?.ok && referrerEnvelope.data ? referrerEnvelope.data.items ?? [] : []);
      setError(null);
      setLoadingDetail(false);
    })();

    return () => {
      cancelled = true;
    };
  }, [visible, selectedRelPath]);

  const filteredPdfs = useMemo(() => {
    const keyword = filterText.trim().toLowerCase();
    if (!keyword) {
      return pdfAttachments;
    }

    return pdfAttachments.filter((item) =>
      item.relPath.toLowerCase().includes(keyword) ||
      item.basename.toLowerCase().includes(keyword)
    );
  }, [pdfAttachments, filterText]);

  return (
    <ToolWorkspaceShell
      sidebar={
        <>
          <ToolSection title="PDF 目录" subtitle={`共 ${pdfAttachments.length} 个 PDF`}>
            <input
              value={filterText}
              onChange={(event) => setFilterText(event.target.value)}
              placeholder="按路径筛选 PDF"
              className="w-full rounded-[10px] px-3 py-2 text-[13px] outline-none border-[0.5px] border-[var(--panel-border)] bg-[var(--subtle-surface)] text-[var(--text-primary)]"
            />
            <div className="mt-3 space-y-1">
              {loadingList ? (
                <div className="px-3 py-3 text-[12px] text-[var(--text-quaternary)]">正在读取 PDF 目录…</div>
              ) : filteredPdfs.length > 0 ? (
                filteredPdfs.map((item) => (
                  <ToolListButton
                    key={item.relPath}
                    title={item.basename}
                    subtitle={item.relPath}
                    active={item.relPath === selectedRelPath}
                    onClick={() => setSelectedRelPath(item.relPath)}
                    eyebrow="PDF"
                    trailing={<ToolBadge label={`${item.refCount} 引用`} />}
                  />
                ))
              ) : (
                <div className="px-3 py-3 text-[12px] text-[var(--text-quaternary)]">当前没有 PDF 附件。</div>
              )}
            </div>
          </ToolSection>

          <ToolSection
            title="当前笔记 PDF 引用"
            subtitle={activeNote ? activeNote.relPath : "先在文件区打开一篇笔记"}
            action={activeNote ? <ToolBadge label={`${noteRefs.length} 个`} /> : undefined}
          >
            {activeNote ? (
              noteRefs.length > 0 ? (
                <div className="space-y-1">
                  {noteRefs.map((ref) => (
                    <ToolListButton
                      key={`${ref.pdfRelPath}-${ref.anchorSerialized}`}
                      title={ref.pdfRelPath}
                      subtitle={ref.excerptText || ref.anchorSerialized || `page ${ref.page}`}
                      active={ref.pdfRelPath === selectedRelPath}
                      onClick={() => setSelectedRelPath(ref.pdfRelPath)}
                    />
                  ))}
                </div>
              ) : (
                <div className="text-[12px] text-[var(--text-quaternary)]">当前笔记没有引用 PDF。</div>
              )
            ) : (
              <div className="text-[12px] text-[var(--text-quaternary)]">打开笔记后，这里会显示它引用的 PDF。</div>
            )}
          </ToolSection>
        </>
      }
    >
      {error ? (
        <div className="p-6">
          <ToolErrorBanner message={error} />
        </div>
      ) : !metadata ? (
        <ToolEmptyState
          title="暂无 PDF"
          description="这个仓库里还没有被笔记引用的 PDF。打开包含 PDF 链接的笔记后，这里会显示文档信息和引用关系。"
        />
      ) : (
        <div className="flex flex-col min-h-full">
          <ToolContentHeader
            title={metadata.docTitle || metadata.relPath.split("/").pop() || metadata.relPath}
            subtitle={metadata.relPath}
            badges={
              <>
                <ToolBadge label={`${metadata.pageCount} 页`} />
                <ToolBadge label={formatPresenceLabel(metadata.presence)} />
              </>
            }
          />

          <ToolBody>
            {loadingDetail ? (
              <div className="text-[13px] text-[var(--text-quaternary)]">正在读取 PDF 信息…</div>
            ) : null}

            <ToolDetailSection title="摘要" subtitle="PDF 附件的基础文档信息。">
              <div className="grid grid-cols-1 md:grid-cols-3 gap-3">
                <ToolMetric label="页数" value={String(metadata.pageCount)} />
                <ToolMetric label="大纲" value={metadata.hasOutline ? "存在" : "无"} />
                <ToolMetric label="标题" value={metadata.docTitle || "未提取"} />
              </div>
            </ToolDetailSection>

            <ToolDevDetails subtitle="默认收起 PDF 详细字段和状态码，避免主界面变成调试面板。">
              <ToolMetaGrid
                items={[
                  { label: "rel_path", value: metadata.relPath },
                  { label: "doc_title", value: metadata.docTitle || "(empty)" },
                  { label: "pdf_metadata_revision", value: metadata.pdfMetadataRevision || "(none)" },
                  { label: "page_count", value: String(metadata.pageCount) },
                  { label: "has_outline", value: String(metadata.hasOutline) },
                  { label: "presence", value: String(metadata.presence) },
                  { label: "metadata_state", value: String(metadata.metadataState) },
                  { label: "doc_title_state", value: String(metadata.docTitleState) },
                  { label: "text_layer_state", value: String(metadata.textLayerState) }
                ]}
              />
            </ToolDevDetails>

            <ToolDetailSection title="引用它的笔记" subtitle="哪些笔记引用了这个 PDF。">
              {referrers.length > 0 ? (
                <div className="space-y-2">
                  {referrers.map((ref) => (
                    <ToolReferenceCard
                      key={`${ref.noteRelPath}-${ref.anchorSerialized}-${ref.page}`}
                      title={ref.noteTitle || ref.noteRelPath}
                      subtitle={ref.noteRelPath}
                      meta={`第 ${ref.page} 页`}
                      action={
                        <ToolActionButton onClick={() => onOpenNote(ref.noteRelPath)}>
                          打开笔记
                        </ToolActionButton>
                      }
                    >
                      {ref.excerptText || ref.anchorSerialized || "(无引用预览)"}
                    </ToolReferenceCard>
                  ))}
                </div>
              ) : (
                <div className="text-[12px] text-[var(--text-quaternary)]">当前 PDF 还没有被笔记引用。</div>
              )}
            </ToolDetailSection>
          </ToolBody>
        </div>
      )}
    </ToolWorkspaceShell>
  );
}
