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
  formatAttachmentKind,
  formatAttachmentKindLabel,
  formatBytes,
  formatNsTimestamp,
  formatPresenceLabel
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
                    eyebrow={formatAttachmentKindLabel(attachment.kind)}
                    trailing={<ToolBadge label={`${attachment.refCount} 引用`} />}
                  />
                ))
              ) : (
                <div className="px-3 py-3 text-[12px] text-[var(--text-quaternary)]">当前没有附件。</div>
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
                <div className="text-[12px] text-[var(--text-quaternary)]">当前笔记没有引用附件。</div>
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
          title="暂无附件"
          description="这个仓库里还没有被笔记引用的附件。打开包含附件链接的笔记后，这里会显示附件列表和引用关系。"
        />
      ) : (
        <div className="flex flex-col min-h-full">
          <ToolContentHeader
            title={selectedAttachment.basename}
            subtitle={selectedAttachment.relPath}
            badges={
              <>
                <ToolBadge label={`${selectedAttachment.refCount} 个引用`} />
                <ToolBadge label={formatPresenceLabel(selectedAttachment.presence)} />
                <ToolBadge label={formatAttachmentKindLabel(selectedAttachment.kind)} />
              </>
            }
          />

          <ToolBody>
            {loadingDetail ? (
              <div className="text-[13px] text-[var(--text-quaternary)]">正在读取附件详情…</div>
            ) : null}

            <ToolDetailSection title="摘要" subtitle="当前附件的基础信息。">
              <div className="grid grid-cols-1 md:grid-cols-3 gap-3">
                <ToolMetric label="大小" value={formatBytes(selectedAttachment.fileSize)} />
                <ToolMetric label="修改时间" value={formatNsTimestamp(selectedAttachment.mtimeNs)} />
                <ToolMetric label="引用数" value={String(selectedAttachment.refCount)} hint={formatPresenceLabel(selectedAttachment.presence)} />
              </div>
            </ToolDetailSection>

            <ToolDevDetails subtitle="默认收起详细字段；需要排查接线时再展开。">
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
            </ToolDevDetails>

            <ToolDetailSection title="引用它的笔记" subtitle="哪些笔记引用了这个附件。">
              {referrers.length > 0 ? (
                <div className="space-y-2">
                  {referrers.map((referrer) => (
                    <ToolReferenceCard
                      key={`${referrer.noteRelPath}-${referrer.noteTitle}`}
                      title={referrer.noteTitle || referrer.noteRelPath}
                      subtitle={referrer.noteRelPath}
                      action={
                        <ToolActionButton onClick={() => onOpenNote(referrer.noteRelPath)}>
                          打开笔记
                        </ToolActionButton>
                      }
                    />
                  ))}
                </div>
              ) : (
                <div className="text-[12px] text-[var(--text-quaternary)]">当前附件还没有被笔记引用。</div>
              )}
            </ToolDetailSection>
          </ToolBody>
        </div>
      )}
    </ToolWorkspaceShell>
  );
}
