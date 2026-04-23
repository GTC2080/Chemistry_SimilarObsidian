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
  ToolContentHeader,
  ToolEmptyState,
  ToolErrorBanner,
  ToolListButton,
  ToolMetaGrid,
  ToolMetric,
  ToolSection,
  ToolWorkspaceShell
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
                <ToolBadge label={`presence ${spectrum.presence}`} />
                <ToolBadge label={`state ${spectrum.state}`} />
                <ToolBadge label={`format ${spectrum.sourceFormat}`} />
              </>
            }
          />

          <div className="p-6 space-y-6">
            {loadingDetail ? (
              <div className="text-[13px] text-[var(--text-quaternary)]">正在读取 spectra 详情…</div>
            ) : null}

            <section>
              <div className="grid grid-cols-1 md:grid-cols-3 gap-3">
                <ToolMetric label="Subtype revision" value={String(spectrum.subtypeRevision)} />
                <ToolMetric label="Coarse kind" value={String(spectrum.coarseKind)} />
                <ToolMetric label="Flags" value={String(spectrum.flags)} />
              </div>
            </section>

            <section>
              <h2 className="text-[13px] font-semibold mb-3 text-[var(--text-primary)]">Subtype record</h2>
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
            </section>

            <section>
              <h2 className="text-[13px] font-semibold mb-3 text-[var(--text-primary)]">Domain metadata</h2>
              {metadataEntries.length > 0 ? (
                <div className="space-y-2">
                  {metadataEntries.map((entry) => (
                    <div
                      key={`${entry.namespace}.${entry.keyName}`}
                      className="rounded-[12px] px-4 py-3 bg-[var(--subtle-surface)] border-[0.5px] border-[var(--panel-border)]"
                    >
                      <div className="flex items-center justify-between gap-3">
                        <div className="min-w-0">
                          <div className="text-[13px] font-medium text-[var(--text-secondary)]">
                            {entry.namespace}.{entry.keyName}
                          </div>
                          <div className="text-[11px] mt-1 truncate text-[var(--text-quaternary)]">
                            carrier {entry.carrierKind} · schema {entry.publicSchemaRevision} · valueKind {entry.valueKind}
                          </div>
                        </div>
                        <ToolBadge label={`flags ${entry.flags}`} />
                      </div>
                      <div className="text-[12px] mt-3 break-all text-[var(--text-tertiary)]">
                        {displayMetadataValue(entry)}
                      </div>
                    </div>
                  ))}
                </div>
              ) : (
                <div className="text-[12px] text-[var(--text-quaternary)]">当前谱图没有公开 domain metadata。</div>
              )}
            </section>

            <section>
              <h2 className="text-[13px] font-semibold mb-3 text-[var(--text-primary)]">Referrers</h2>
              {referrers.length > 0 ? (
                <div className="space-y-2">
                  {referrers.map((ref) => (
                    <div
                      key={`${ref.noteRelPath}-${ref.selectorSerialized}`}
                      className="rounded-[12px] px-4 py-3 bg-[var(--subtle-surface)] border-[0.5px] border-[var(--panel-border)]"
                    >
                      <div className="flex items-start justify-between gap-3">
                        <div className="min-w-0">
                          <div className="text-[13px] font-medium truncate text-[var(--text-secondary)]">
                            {ref.noteTitle || ref.noteRelPath}
                          </div>
                          <div className="text-[11px] mt-1 truncate text-[var(--text-quaternary)]">
                            {ref.noteRelPath}
                          </div>
                        </div>
                        <ToolActionButton onClick={() => onOpenNote(ref.noteRelPath)}>
                          打开笔记
                        </ToolActionButton>
                      </div>
                      <div className="text-[12px] mt-3 text-[var(--text-tertiary)]">
                        selector {ref.selectorKind} · state {ref.state}
                      </div>
                      <div className="text-[11px] mt-1 break-all text-[var(--text-quaternary)]">
                        {ref.previewText || ref.selectorSerialized || ref.targetBasisRevision || "(no preview text)"}
                      </div>
                    </div>
                  ))}
                </div>
              ) : (
                <div className="text-[12px] text-[var(--text-quaternary)]">当前谱图还没有 formal chemistry referrers。</div>
              )}
            </section>
          </div>
        </div>
      )}
    </ToolWorkspaceShell>
  );
}
