import { useEffect, useMemo, useState } from "react";
import type { NoteRecord } from "../../lib/files-tree";
import {
  getChemistrySpectrum,
  listChemistrySpectra,
  queryChemistryMetadata,
  queryChemistryReferrers,
  queryNoteChemistryRefs,
  type HostChemSpectrumRecord,
  type HostChemSpectrumReferrer,
  type HostChemSpectrumSourceRef,
  type HostDomainMetadataEntry
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
  formatChemSourceFormat,
  formatPresenceLabel
} from "./ToolingScaffold";

interface ChemistryWorkspaceProps {
  visible: boolean;
  activeNote: NoteRecord | null;
  onOpenNote: (relPath: string) => void;
}

function displayMetadataValue(entry: HostDomainMetadataEntry) {
  if (entry.stringValue) {
    return entry.stringValue;
  }
  if (entry.uint64Value) {
    return String(entry.uint64Value);
  }
  return String(entry.boolValue);
}

function displayMetadataLabel(entry: HostDomainMetadataEntry) {
  if (entry.keyName === "family") {
    return "谱图类型";
  }
  if (entry.keyName === "point_count") {
    return "数据点";
  }
  if (entry.keyName === "sample_label") {
    return "样品标签";
  }
  if (entry.keyName === "source_format") {
    return "来源格式";
  }
  if (entry.keyName === "x_axis_unit") {
    return "X 轴单位";
  }
  if (entry.keyName === "y_axis_unit") {
    return "Y 轴单位";
  }
  return entry.keyName;
}

export default function ChemistryWorkspace({
  visible,
  activeNote,
  onOpenNote
}: ChemistryWorkspaceProps) {
  const [spectra, setSpectra] = useState<HostChemSpectrumRecord[]>([]);
  const [selectedRelPath, setSelectedRelPath] = useState<string | null>(null);
  const [spectrum, setSpectrum] = useState<HostChemSpectrumRecord | null>(null);
  const [metadataEntries, setMetadataEntries] = useState<HostDomainMetadataEntry[]>([]);
  const [referrers, setReferrers] = useState<HostChemSpectrumReferrer[]>([]);
  const [noteRefs, setNoteRefs] = useState<HostChemSpectrumSourceRef[]>([]);
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
      const envelope = await listChemistrySpectra();
      if (cancelled) {
        return;
      }

      if (!envelope?.ok || !envelope.data) {
        setSpectra([]);
        setError(envelope?.error?.message ?? "读取 spectra 目录失败。");
        setLoadingList(false);
        return;
      }

      const items = envelope.data.items ?? [];
      setSpectra(items);
      setError(null);
      setLoadingList(false);
      if (!selectedRelPath && items.length > 0) {
        setSelectedRelPath(items[0].attachmentRelPath);
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
      const envelope = await queryNoteChemistryRefs(activeNote.relPath);
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
      setSpectrum(null);
      setMetadataEntries([]);
      setReferrers([]);
      return;
    }

    let cancelled = false;
    void (async () => {
      setLoadingDetail(true);
      const [spectrumEnvelope, metadataEnvelope, referrerEnvelope] = await Promise.all([
        getChemistrySpectrum(selectedRelPath),
        queryChemistryMetadata(selectedRelPath),
        queryChemistryReferrers(selectedRelPath)
      ]);

      if (cancelled) {
        return;
      }

      if (!spectrumEnvelope?.ok || !spectrumEnvelope.data) {
        setSpectrum(null);
        setMetadataEntries([]);
        setReferrers([]);
        setError(spectrumEnvelope?.error?.message ?? "读取 spectra 详情失败。");
        setLoadingDetail(false);
        return;
      }

      setSpectrum(spectrumEnvelope.data);
      setMetadataEntries(metadataEnvelope?.ok && metadataEnvelope.data ? metadataEnvelope.data.items ?? [] : []);
      setReferrers(referrerEnvelope?.ok && referrerEnvelope.data ? referrerEnvelope.data.items ?? [] : []);
      setError(null);
      setLoadingDetail(false);
    })();

    return () => {
      cancelled = true;
    };
  }, [visible, selectedRelPath]);

  const filteredSpectra = useMemo(() => {
    const keyword = filterText.trim().toLowerCase();
    if (!keyword) {
      return spectra;
    }

    return spectra.filter((item) =>
      item.attachmentRelPath.toLowerCase().includes(keyword) ||
      item.domainObjectKey.toLowerCase().includes(keyword)
    );
  }, [spectra, filterText]);

  return (
    <ToolWorkspaceShell
      sidebar={
        <>
          <ToolSection title="Spectra 目录" subtitle={`共 ${spectra.length} 个谱图对象`}>
            <input
              value={filterText}
              onChange={(event) => setFilterText(event.target.value)}
              placeholder="按 carrier 或 object key 筛选"
              className="w-full rounded-[10px] px-3 py-2 text-[13px] outline-none border-[0.5px] border-[var(--panel-border)] bg-[var(--subtle-surface)] text-[var(--text-primary)]"
            />
            <div className="mt-3 space-y-1">
              {loadingList ? (
                <div className="px-3 py-3 text-[12px] text-[var(--text-quaternary)]">正在读取 spectra 目录…</div>
              ) : filteredSpectra.length > 0 ? (
                filteredSpectra.map((item) => (
                  <ToolListButton
                    key={item.attachmentRelPath}
                    title={item.attachmentRelPath.split("/").pop() || item.attachmentRelPath}
                    subtitle={item.domainObjectKey || item.attachmentRelPath}
                    active={item.attachmentRelPath === selectedRelPath}
                    onClick={() => setSelectedRelPath(item.attachmentRelPath)}
                    eyebrow={formatChemSourceFormat(item.sourceFormat)}
                    trailing={<ToolBadge label={formatPresenceLabel(item.presence)} />}
                  />
                ))
              ) : (
                <div className="px-3 py-3 text-[12px] text-[var(--text-quaternary)]">当前没有 chemistry spectra。</div>
              )}
            </div>
          </ToolSection>

          <ToolSection
            title="当前笔记 spectra refs"
            subtitle={activeNote ? activeNote.relPath : "先在文件区打开一篇笔记"}
            action={activeNote ? <ToolBadge label={`${noteRefs.length} 个`} /> : undefined}
          >
            {activeNote ? (
              noteRefs.length > 0 ? (
                <div className="space-y-1">
                  {noteRefs.map((ref) => (
                    <ToolListButton
                      key={`${ref.attachmentRelPath}-${ref.selectorSerialized}`}
                      title={ref.attachmentRelPath}
                      subtitle={ref.previewText || ref.selectorSerialized || ref.domainObjectKey}
                      active={ref.attachmentRelPath === selectedRelPath}
                      onClick={() => setSelectedRelPath(ref.attachmentRelPath)}
                    />
                  ))}
                </div>
              ) : (
                <div className="text-[12px] text-[var(--text-quaternary)]">当前笔记还没有 formal chemistry source refs。</div>
              )
            ) : (
              <div className="text-[12px] text-[var(--text-quaternary)]">打开笔记后，这里会显示它的 spectra refs。</div>
            )}
          </ToolSection>
        </>
      }
    >
      {error ? (
        <div className="p-6">
          <ToolErrorBanner message={error} />
        </div>
      ) : !spectrum ? (
        <ToolEmptyState
          title="Chemistry spectra substrate 已接入"
          description="这里直接消费 chemistry spectra public surface。选中谱图对象后，可以查看 subtype record、domain metadata，以及 note ↔ spectrum referrers。"
        />
      ) : (
        <div className="flex flex-col min-h-full">
          <ToolContentHeader
            title={spectrum.attachmentRelPath.split("/").pop() || spectrum.attachmentRelPath}
            subtitle={spectrum.domainObjectKey || spectrum.attachmentRelPath}
            badges={
              <>
                <ToolBadge label={formatPresenceLabel(spectrum.presence)} />
                <ToolBadge label={formatChemSourceFormat(spectrum.sourceFormat)} />
              </>
            }
          />

          <ToolBody>
            {loadingDetail ? (
              <div className="text-[13px] text-[var(--text-quaternary)]">正在读取 spectra 详情…</div>
            ) : null}

            <ToolDetailSection title="谱图摘要" subtitle="当前谱图对象的基础信息。">
              <div className="grid grid-cols-1 md:grid-cols-3 gap-3">
                <ToolMetric label="格式" value={formatChemSourceFormat(spectrum.sourceFormat)} />
                <ToolMetric label="状态" value={formatPresenceLabel(spectrum.presence)} />
                <ToolMetric label="元数据" value={`${metadataEntries.length} 项`} />
              </div>
            </ToolDetailSection>

            <ToolDevDetails subtitle="默认收起 subtype revision、flags 和内部状态码，保留给接线排查使用。">
              <ToolMetaGrid
                items={[
                  { label: "attachment_rel_path", value: spectrum.attachmentRelPath },
                  { label: "domain_object_key", value: spectrum.domainObjectKey || "(empty)" },
                  { label: "subtype_revision", value: String(spectrum.subtypeRevision) },
                  { label: "source_format", value: String(spectrum.sourceFormat) },
                  { label: "coarse_kind", value: String(spectrum.coarseKind) },
                  { label: "presence", value: String(spectrum.presence) },
                  { label: "state", value: String(spectrum.state) },
                  { label: "flags", value: String(spectrum.flags) }
                ]}
              />
            </ToolDevDetails>

            <ToolDetailSection title="谱图信息" subtitle="来自 chem.spectrum.* 的稳定公开字段。">
              {metadataEntries.length > 0 ? (
                <div className="space-y-2">
                  {metadataEntries.map((entry) => (
                    <ToolReferenceCard
                      key={`${entry.namespace}.${entry.keyName}`}
                      title={displayMetadataLabel(entry)}
                      subtitle={`${entry.namespace}.${entry.keyName}`}
                      meta={displayMetadataValue(entry)}
                    />
                  ))}
                </div>
              ) : (
                <div className="text-[12px] text-[var(--text-quaternary)]">当前谱图没有公开 domain metadata。</div>
              )}
            </ToolDetailSection>

            <ToolDetailSection title="Referrers" subtitle="Formal note ↔ spectrum refs，不扩展为化学工作流壳层。">
              {referrers.length > 0 ? (
                <div className="space-y-2">
                  {referrers.map((ref) => (
                    <ToolReferenceCard
                      key={`${ref.noteRelPath}-${ref.selectorSerialized}`}
                      title={ref.noteTitle || ref.noteRelPath}
                      subtitle={ref.noteRelPath}
                      meta={ref.previewText ? "包含谱图引用预览" : "谱图引用"}
                      action={
                        <ToolActionButton onClick={() => onOpenNote(ref.noteRelPath)}>
                          打开笔记
                        </ToolActionButton>
                      }
                    >
                      {ref.previewText || ref.selectorSerialized || ref.targetBasisRevision || "(no preview text)"}
                    </ToolReferenceCard>
                  ))}
                </div>
              ) : (
                <div className="text-[12px] text-[var(--text-quaternary)]">当前谱图还没有 formal chemistry referrers。</div>
              )}
            </ToolDetailSection>
          </ToolBody>
        </div>
      )}
    </ToolWorkspaceShell>
  );
}
