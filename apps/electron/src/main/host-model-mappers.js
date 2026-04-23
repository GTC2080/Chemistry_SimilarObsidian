const {
  HOST_INDEX_STATES
} = require("../shared/host-contract");

function mapKernelRuntimeSummary(summary = {}) {
  return {
    sessionState: summary.session_state ?? "closed",
    indexState: summary.index_state ?? HOST_INDEX_STATES.unavailable,
    indexedNoteCount: Number(summary.indexed_note_count ?? 0),
    pendingRecoveryOps: Number(summary.pending_recovery_ops ?? 0)
  };
}

function mapKernelRebuildStatus(summary = {}) {
  return {
    inFlight: Boolean(summary.in_flight),
    hasLastResult: Boolean(summary.has_last_result),
    currentGeneration: Number(summary.current_generation ?? 0),
    lastCompletedGeneration: Number(summary.last_completed_generation ?? 0),
    currentStartedAtNs: Number(summary.current_started_at_ns ?? 0),
    lastResultCode: summary.last_result_code ?? null,
    lastResultAtNs: Number(summary.last_result_at_ns ?? 0),
    indexState: summary.index_state ?? HOST_INDEX_STATES.unavailable
  };
}

function mapSearchHit(hit = {}) {
  return {
    relPath: hit.rel_path ?? "",
    title: hit.title ?? "",
    snippet: hit.snippet ?? "",
    matchFlags: Number(hit.match_flags ?? 0),
    snippetStatus: hit.snippet_status ?? 0,
    resultKind: hit.result_kind ?? 0,
    resultFlags: Number(hit.result_flags ?? 0),
    score: Number(hit.score ?? 0)
  };
}

function mapSearchPage(page = {}, request = {}) {
  return {
    request: {
      query: request.query,
      limit: request.limit,
      offset: request.offset,
      kind: request.kind,
      tagFilter: request.tagFilter ?? null,
      pathPrefix: request.pathPrefix ?? null,
      includeDeleted: Boolean(request.includeDeleted),
      sortMode: request.sortMode
    },
    count: Array.isArray(page.hits) ? page.hits.length : Number(page.count ?? 0),
    totalHits: Number(page.total_hits ?? 0),
    hasMore: Boolean(page.has_more),
    items: Array.isArray(page.hits) ? page.hits.map(mapSearchHit) : []
  };
}

function mapFilesEntry(entry = {}) {
  return {
    relPath: entry.rel_path ?? "",
    name: entry.name ?? "",
    title: entry.title ?? "",
    kind: entry.kind ?? "unknown",
    isDirectory: Boolean(entry.is_directory),
    sizeBytes: Number(entry.size_bytes ?? 0),
    mtimeMs: Number(entry.mtime_ms ?? 0)
  };
}

function mapFilesList(list = {}, request = {}) {
  return {
    parentRelPath: request.parentRelPath ?? null,
    count: Array.isArray(list.entries) ? list.entries.length : Number(list.count ?? 0),
    items: Array.isArray(list.entries) ? list.entries.map(mapFilesEntry) : []
  };
}

function mapFileOperationResult(result = {}) {
  return {
    disposition: result.disposition ?? "ok",
    deleted: Boolean(result.deleted),
    relPath: result.rel_path ?? result.relPath ?? result.entry?.rel_path ?? "",
    kind: result.kind ?? result.entry?.kind ?? "unknown",
    isDirectory: Boolean(result.is_directory ?? result.entry?.is_directory),
    entry: result.entry ? mapFilesEntry(result.entry) : null
  };
}

function basenameFromRelPath(relPath = "") {
  const parts = String(relPath).split("/").filter(Boolean);
  return parts.length > 0 ? parts[parts.length - 1] : "";
}

function titleFromName(name = "") {
  return String(name).replace(/\.[^.]+$/, "") || String(name);
}

function titleFromBody(name = "", bodyText = "") {
  const heading = String(bodyText)
    .split(/\r?\n/)
    .map((line) => line.trim())
    .find((line) => line.startsWith("#") && line.replace(/^#+\s*/, "").trim());
  if (heading) {
    return heading.replace(/^#+\s*/, "").trim();
  }

  return titleFromName(name);
}

function mapFileNoteRecord(record = {}) {
  const relPath = record.rel_path ?? record.relPath ?? "";
  const name = record.name ?? basenameFromRelPath(relPath);
  const bodyText = record.body_text ?? record.bodyText ?? "";
  const mtimeNs = Number(record.mtime_ns ?? record.mtimeNs ?? 0);
  return {
    relPath,
    name,
    title: record.title ?? titleFromBody(name, bodyText),
    kind: record.kind ?? "note",
    bodyText,
    sizeBytes: Number(record.size_bytes ?? record.sizeBytes ?? record.file_size ?? 0),
    mtimeMs: Number(record.mtime_ms ?? record.mtimeMs ?? (mtimeNs > 0 ? Math.floor(mtimeNs / 1_000_000) : 0)),
    contentRevision: record.content_revision ?? record.contentRevision ?? ""
  };
}

function mapNoteWriteResult(result = {}) {
  return {
    disposition: result.disposition ?? "written",
    note: mapFileNoteRecord(result.note ?? result)
  };
}

function mapAttachmentRecord(record = {}) {
  return {
    relPath: record.rel_path ?? "",
    basename: record.basename ?? "",
    extension: record.extension ?? "",
    fileSize: Number(record.file_size ?? 0),
    mtimeNs: Number(record.mtime_ns ?? 0),
    refCount: Number(record.ref_count ?? 0),
    kind: record.kind ?? 0,
    flags: Number(record.flags ?? 0),
    presence: record.presence ?? 0
  };
}

function mapAttachmentList(list = {}) {
  return {
    count: Array.isArray(list.attachments) ? list.attachments.length : Number(list.count ?? 0),
    items: Array.isArray(list.attachments) ? list.attachments.map(mapAttachmentRecord) : []
  };
}

function mapAttachmentReferrers(referrers = {}, request = {}) {
  return {
    attachmentRelPath: request.attachmentRelPath ?? null,
    count: Array.isArray(referrers.referrers) ? referrers.referrers.length : Number(referrers.count ?? 0),
    items: Array.isArray(referrers.referrers)
      ? referrers.referrers.map((entry) => ({
        noteRelPath: entry.note_rel_path ?? "",
        noteTitle: entry.note_title ?? ""
      }))
      : []
  };
}

function mapDomainMetadataEntry(entry = {}) {
  return {
    carrierKind: entry.carrier_kind ?? 0,
    carrierKey: entry.carrier_key ?? "",
    namespace: entry.namespace_name ?? "",
    publicSchemaRevision: Number(entry.public_schema_revision ?? 0),
    keyName: entry.key_name ?? "",
    valueKind: entry.value_kind ?? 0,
    boolValue: Boolean(entry.bool_value),
    uint64Value: Number(entry.uint64_value ?? 0),
    stringValue: entry.string_value ?? "",
    flags: Number(entry.flags ?? 0)
  };
}

function mapDomainMetadataList(list = {}, request = {}) {
  return {
    attachmentRelPath: request.attachmentRelPath ?? null,
    count: Array.isArray(list.entries) ? list.entries.length : Number(list.count ?? 0),
    items: Array.isArray(list.entries) ? list.entries.map(mapDomainMetadataEntry) : []
  };
}

function mapPdfMetadata(record = {}) {
  return {
    relPath: record.rel_path ?? "",
    docTitle: record.doc_title ?? "",
    pdfMetadataRevision: record.pdf_metadata_revision ?? "",
    pageCount: Number(record.page_count ?? 0),
    hasOutline: Boolean(record.has_outline),
    presence: record.presence ?? 0,
    metadataState: record.metadata_state ?? 0,
    docTitleState: record.doc_title_state ?? 0,
    textLayerState: record.text_layer_state ?? 0
  };
}

function mapPdfSourceRef(ref = {}) {
  return {
    pdfRelPath: ref.pdf_rel_path ?? "",
    anchorSerialized: ref.anchor_serialized ?? "",
    excerptText: ref.excerpt_text ?? "",
    page: Number(ref.page ?? 0),
    state: ref.state ?? 0
  };
}

function mapPdfSourceRefs(list = {}, request = {}) {
  return {
    noteRelPath: request.noteRelPath ?? null,
    count: Array.isArray(list.refs) ? list.refs.length : Number(list.count ?? 0),
    items: Array.isArray(list.refs) ? list.refs.map(mapPdfSourceRef) : []
  };
}

function mapPdfReferrers(list = {}, request = {}) {
  return {
    attachmentRelPath: request.attachmentRelPath ?? null,
    count: Array.isArray(list.referrers) ? list.referrers.length : Number(list.count ?? 0),
    items: Array.isArray(list.referrers)
      ? list.referrers.map((entry) => ({
        noteRelPath: entry.note_rel_path ?? "",
        noteTitle: entry.note_title ?? "",
        anchorSerialized: entry.anchor_serialized ?? "",
        excerptText: entry.excerpt_text ?? "",
        page: Number(entry.page ?? 0),
        state: entry.state ?? 0
      }))
      : []
  };
}

function mapChemSpectrumRecord(record = {}) {
  return {
    attachmentRelPath: record.attachment_rel_path ?? "",
    domainObjectKey: record.domain_object_key ?? "",
    subtypeRevision: Number(record.subtype_revision ?? 0),
    sourceFormat: record.source_format ?? 0,
    coarseKind: record.coarse_kind ?? 0,
    presence: record.presence ?? 0,
    state: record.state ?? 0,
    flags: Number(record.flags ?? 0)
  };
}

function mapChemSpectrumList(list = {}) {
  return {
    count: Array.isArray(list.spectra) ? list.spectra.length : Number(list.count ?? 0),
    items: Array.isArray(list.spectra) ? list.spectra.map(mapChemSpectrumRecord) : []
  };
}

function mapChemSpectrumSourceRef(ref = {}) {
  return {
    attachmentRelPath: ref.attachment_rel_path ?? "",
    domainObjectKey: ref.domain_object_key ?? "",
    selectorKind: ref.selector_kind ?? 0,
    selectorSerialized: ref.selector_serialized ?? "",
    previewText: ref.preview_text ?? "",
    targetBasisRevision: ref.target_basis_revision ?? "",
    state: ref.state ?? 0,
    flags: Number(ref.flags ?? 0)
  };
}

function mapChemSpectrumSourceRefs(list = {}, request = {}) {
  return {
    noteRelPath: request.noteRelPath ?? null,
    count: Array.isArray(list.refs) ? list.refs.length : Number(list.count ?? 0),
    items: Array.isArray(list.refs) ? list.refs.map(mapChemSpectrumSourceRef) : []
  };
}

function mapChemSpectrumReferrers(list = {}, request = {}) {
  return {
    attachmentRelPath: request.attachmentRelPath ?? null,
    count: Array.isArray(list.referrers) ? list.referrers.length : Number(list.count ?? 0),
    items: Array.isArray(list.referrers)
      ? list.referrers.map((entry) => ({
        noteRelPath: entry.note_rel_path ?? "",
        noteTitle: entry.note_title ?? "",
        attachmentRelPath: entry.attachment_rel_path ?? "",
        domainObjectKey: entry.domain_object_key ?? "",
        selectorKind: entry.selector_kind ?? 0,
        selectorSerialized: entry.selector_serialized ?? "",
        previewText: entry.preview_text ?? "",
        targetBasisRevision: entry.target_basis_revision ?? "",
        state: entry.state ?? 0,
        flags: Number(entry.flags ?? 0)
      }))
      : []
  };
}

function mapDiagnosticsExport(result = {}, request = {}) {
  return {
    outputPath: request.outputPath ?? null,
    result: result.result ?? "exported"
  };
}

module.exports = {
  mapAttachmentList,
  mapAttachmentRecord,
  mapAttachmentReferrers,
  mapChemSpectrumList,
  mapChemSpectrumRecord,
  mapChemSpectrumReferrers,
  mapChemSpectrumSourceRefs,
  mapDiagnosticsExport,
  mapDomainMetadataList,
  mapFileNoteRecord,
  mapFileOperationResult,
  mapFilesList,
  mapKernelRebuildStatus,
  mapKernelRuntimeSummary,
  mapNoteWriteResult,
  mapPdfMetadata,
  mapPdfReferrers,
  mapPdfSourceRefs,
  mapSearchPage
};
