import { useEffect, useMemo, useState } from "react";
import type { NoteRecord } from "../../lib/files-tree";
import {
  getAttachment,
  listAttachments,
  queryAttachmentReferrers,
  queryNoteAttachmentRefs,
  type HostAttachmentRecord,
  type HostAttachmentReferrer
} from "../../lib/host-shell";
import {
  ToolActionButton,
  ToolBadge,
  ToolContentHeader,
  ToolEmptyState,
  ToolErrorBanner,
  ToolListButton,
  ToolMetaGrid,
  ToolMetric,
  ToolSection,
  ToolWorkspaceShell,
  formatBytes,
  formatNsTimestamp
} from "./ToolingScaffold";

interface AttachmentsWorkspaceProps {
  visible: boolean;
  activeNote: NoteRecord | null;
  onOpenNote: (relPath: string) => void;
}

export default function AttachmentsWorkspace({
  visible,
  activeNote,
  onOpenNote
}: AttachmentsWorkspaceProps) {
  const [attachments, setAttachments] = useState<HostAttachmentRecord[]>([]);
  const [selectedRelPath, setSelectedRelPath] = useState<string | null>(null);
  const [selectedAttachment, setSelectedAttachment] = useState<HostAttachmentRecord | null>(null);
  const [referrers, setReferrers] = useState<HostAttachmentReferrer[]>([]);
  const [currentNoteRefs, setCurrentNoteRefs] = useState<HostAttachmentRecord[]>([]);
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
        setAttachments([]);
        setError(envelope?.error?.message ?? "读取附件目录失败。");
        setLoadingList(false);
        return;
      }

      const items = envelope.data.items ?? [];
      setAttachments(items);
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
      setCurrentNoteRefs([]);
      return;
    }

    let cancelled = false;
    void (async () => {
      const envelope = await queryNoteAttachmentRefs(activeNote.relPath);
      if (!cancelled) {
        setCurrentNoteRefs(envelope?.ok && envelope.data ? envelope.data.items ?? [] : []);
      }
    })();

    return () => {
      cancelled = true;
    };
  }, [visible, activeNote?.relPath]);

  useEffect(() => {
    if (!visible || !selectedRelPath) {
      setSelectedAttachment(null);
      setReferrers([]);
      return;
    }

    let cancelled = false;
    void (async () => {
      setLoadingDetail(true);
      const [attachmentEnvelope, referrerEnvelope] = await Promise.all([
        getAttachment(selectedRelPath),
        queryAttachmentReferrers(selectedRelPath)
      ]);

      if (cancelled) {
        return;
      }

      if (!attachmentEnvelope?.ok || !attachmentEnvelope.data) {
        setSelectedAttachment(null);
        setReferrers([]);
        setError(attachmentEnvelope?.error?.message ?? "读取附件详情失败。");
        setLoadingDetail(false);
        return;
      }

      setSelectedAttachment(attachmentEnvelope.data);
      setReferrers(referrerEnvelope?.ok && referrerEnvelope.data ? referrerEnvelope.data.items ?? [] : []);
      setError(null);
      setLoadingDetail(false);
    })();

    return () => {
      cancelled = true;
    };
  }, [visible, selectedRelPath]);

  const filteredAttachments = useMemo(() => {
    const keyword = filterText.trim().toLowerCase();
    if (!keyword) {
      return attachments;
    }
    return attachments.filter((item) =>
      item.relPath.toLowerCase().includes(keyword) ||
      item.basename.toLowerCase().includes(keyword)
    );
  }, [attachments, filterText]);

  return (
    <ToolWorkspaceShell
      sidebar={
        <>
          <ToolSection title="附件目录" subtitle={`共 ${attachments.length} 个附件`}>
            <input
              value={filterText}
              onChange={(event) => setFilterText(event.target.value)}
              placeholder="按路径筛选附件"
              className="w-full rounded-[10px] px-3 py-2 text-[13px] outline-none border-[0.5px] border-[var(--panel-border)] bg-[var(--subtle-surface)] text-[var(--text-primary)]"
            />
            <div className="mt-3 space-y-1">
              {loadingList ? (
                <div className="px-3 py-3 text-[12px] text-[var(--text-quaternary)]">正在读取附件目录…</div>
              ) : filteredAttachments.length > 0 ? (
                filteredAttachments.map((attachment) => (
                  <ToolListButton
                    key={attachment.relPath}
                    title={attachment.basename}
                    subtitle={attachment.relPath}
                    active={attachment.relPath === selectedRelPath}
                    onClick={() => setSelectedRelPath(attachment.relPath)}
                    trailing={<span className="text-[10px] text-[var(--text-quaternary)]">{attachment.refCount}</span>}
                  />
                ))
              ) : (
                <div className="px-3 py-3 text-[12px] text-[var(--text-quaternary)]">当前没有可展示的附件。</div>
              )}
            </div>
          </ToolSection>

          <ToolSection
            title="当前笔记引用"
            subtitle={activeNote ? activeNote.relPath : "先在文件区打开一篇笔记"}
            action={activeNote ? <ToolBadge label={`${currentNoteRefs.length} 个`} /> : undefined}
          >
            {activeNote ? (
              currentNoteRefs.length > 0 ? (
                <div className="space-y-1">
                  {currentNoteRefs.map((attachment) => (
                    <ToolListButton
                      key={`note-ref-${attachment.relPath}`}
                      title={attachment.basename}
                      subtitle={attachment.relPath}
                      active={attachment.relPath === selectedRelPath}
                      onClick={() => setSelectedRelPath(attachment.relPath)}
                    />
                  ))}
                </div>
              ) : (
                <div className="text-[12px] text-[var(--text-quaternary)]">当前笔记没有正式 attachment refs。</div>
              )
            ) : (
              <div className="text-[12px] text-[var(--text-quaternary)]">打开笔记后，这里会显示它引用的附件。</div>
            )}
          </ToolSection>
        </>
      }
    >
      {error ? (
        <div className="p-6">
          <ToolErrorBanner message={error} />
        </div>
      ) : !selectedAttachment ? (
        <ToolEmptyState
          title="附件面板已接入"
          description="左侧现在已经直接消费 host 的 attachments public surface。选择一个附件后，可以查看正式 metadata、referrers，以及当前笔记对它的引用。"
        />
      ) : (
        <div className="flex flex-col min-h-full">
          <ToolContentHeader
            title={selectedAttachment.basename}
            subtitle={selectedAttachment.relPath}
            badges={
              <>
                <ToolBadge label={`refs ${selectedAttachment.refCount}`} />
                <ToolBadge label={`presence ${selectedAttachment.presence}`} />
                <ToolBadge label={`kind ${selectedAttachment.kind}`} />
              </>
            }
          />

          <div className="p-6 space-y-6">
            {loadingDetail ? (
              <div className="text-[13px] text-[var(--text-quaternary)]">正在读取附件详情…</div>
            ) : null}

            <section>
              <div className="grid grid-cols-1 md:grid-cols-3 gap-3">
                <ToolMetric label="大小" value={formatBytes(selectedAttachment.fileSize)} />
                <ToolMetric label="修改时间" value={formatNsTimestamp(selectedAttachment.mtimeNs)} />
                <ToolMetric label="Flags" value={String(selectedAttachment.flags)} />
              </div>
            </section>

            <section>
              <h2 className="text-[13px] font-semibold mb-3 text-[var(--text-primary)]">正式 metadata</h2>
              <ToolMetaGrid
                items={[
                  { label: "rel_path", value: selectedAttachment.relPath },
                  { label: "basename", value: selectedAttachment.basename },
                  { label: "extension", value: selectedAttachment.extension || "(none)" },
                  { label: "ref_count", value: String(selectedAttachment.refCount) },
                  { label: "presence", value: String(selectedAttachment.presence) },
                  { label: "kind", value: String(selectedAttachment.kind) },
                  { label: "flags", value: String(selectedAttachment.flags) }
                ]}
              />
            </section>

            <section>
              <h2 className="text-[13px] font-semibold mb-3 text-[var(--text-primary)]">Referrers</h2>
              {referrers.length > 0 ? (
                <div className="space-y-2">
                  {referrers.map((referrer) => (
                    <div
                      key={`${referrer.noteRelPath}-${referrer.noteTitle}`}
                      className="rounded-[12px] px-4 py-3 bg-[var(--subtle-surface)] border-[0.5px] border-[var(--panel-border)] flex items-center justify-between gap-3"
                    >
                      <div className="min-w-0">
                        <div className="text-[13px] font-medium truncate text-[var(--text-secondary)]">
                          {referrer.noteTitle || referrer.noteRelPath}
                        </div>
                        <div className="text-[11px] mt-1 truncate text-[var(--text-quaternary)]">
                          {referrer.noteRelPath}
                        </div>
                      </div>
                      <ToolActionButton onClick={() => onOpenNote(referrer.noteRelPath)}>
                        打开笔记
                      </ToolActionButton>
                    </div>
                  ))}
                </div>
              ) : (
                <div className="text-[12px] text-[var(--text-quaternary)]">当前附件没有 live note referrers。</div>
              )}
            </section>
          </div>
        </div>
      )}
    </ToolWorkspaceShell>
  );
}
