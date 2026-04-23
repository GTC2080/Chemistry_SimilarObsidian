export interface HostEnvelope<T = any> {
  ok: boolean;
  data: T | null;
  error: { code: string; message: string; details: any } | null;
  request_id?: string;
}

export interface HostBootstrapInfo {
  shell: string;
  host_version: string;
  run_mode: "dev" | "packaged";
  packaged: boolean;
  platform: string;
  versions: {
    electron: string;
    chrome: string;
    node: string;
  };
  security: {
    contextIsolation: boolean;
    nodeIntegration: boolean;
    enableRemoteModule: boolean;
    sandbox: boolean;
    rendererDirectNodeAccess: boolean;
    preloadIsOnlyBridge: boolean;
  };
  api_groups: string[];
  renderer_boundary: {
    direct_node_access: boolean;
    direct_electron_access: boolean;
    preload_only_bridge: boolean;
  };
}

export interface HostRuntimeSummary {
  lifecycle_state: string;
  run_mode: "dev" | "packaged";
  main_window: {
    exists: boolean;
    visible: boolean;
  };
  kernel_runtime: {
    session_state: string;
    index_state: string;
    indexed_note_count?: number;
    pending_recovery_ops?: number;
    [key: string]: any;
  };
  rebuild: {
    in_flight: boolean;
    has_last_result: boolean;
    current_generation: number;
    last_completed_generation: number;
    current_started_at_ns: number;
    last_result_code: string | null;
    last_result_at_ns: number;
    index_state: string;
  };
  session: {
    state: string;
    active_vault_path: string | null;
    adapter_attached: boolean;
    last_error: { code: string; message: string; details: any; at_ms: number } | null;
  };
  kernel_binding: {
    attached: boolean;
    failure_code?: string | null;
    failure_message?: string | null;
    load_error?: string | null;
    binary_path?: string | null;
    run_mode?: string | null;
    [key: string]: any;
  };
  last_window_event: {
    kind: string;
    details: any;
    at_ms: number;
  } | null;
}

export interface HostSessionStatus {
  state: string;
  active_vault_path: string | null;
  adapter_attached: boolean;
  last_error: { code: string; message: string; details: any; at_ms: number } | null;
}

export interface HostEntry {
  relPath: string;
  name: string;
  title: string;
  kind: string;
  isDirectory: boolean;
  sizeBytes: number;
  mtimeMs: number;
}

export interface HostNote {
  relPath: string;
  name: string;
  title: string;
  kind: string;
  bodyText: string;
  sizeBytes: number;
  mtimeMs: number;
}

export interface HostAttachmentRecord {
  relPath: string;
  basename: string;
  extension: string;
  fileSize: number;
  mtimeNs: number;
  refCount: number;
  kind: number;
  flags: number;
  presence: number;
}

export interface HostAttachmentReferrer {
  noteRelPath: string;
  noteTitle: string;
}

export interface HostPdfMetadata {
  relPath: string;
  docTitle: string;
  pdfMetadataRevision: string;
  pageCount: number;
  hasOutline: boolean;
  presence: number;
  metadataState: number;
  docTitleState: number;
  textLayerState: number;
}

export interface HostPdfSourceRef {
  pdfRelPath: string;
  anchorSerialized: string;
  excerptText: string;
  page: number;
  state: number;
}

export interface HostPdfReferrer {
  noteRelPath: string;
  noteTitle: string;
  anchorSerialized: string;
  excerptText: string;
  page: number;
  state: number;
}

export interface HostDomainMetadataEntry {
  carrierKind: number;
  carrierKey: string;
  namespace: string;
  publicSchemaRevision: number;
  keyName: string;
  valueKind: number;
  boolValue: boolean;
  uint64Value: number;
  stringValue: string;
  flags: number;
}

export interface HostChemSpectrumRecord {
  attachmentRelPath: string;
  domainObjectKey: string;
  subtypeRevision: number;
  sourceFormat: number;
  coarseKind: number;
  presence: number;
  state: number;
  flags: number;
}

export interface HostChemSpectrumSourceRef {
  attachmentRelPath: string;
  domainObjectKey: string;
  selectorKind: number;
  selectorSerialized: string;
  previewText: string;
  targetBasisRevision: string;
  state: number;
  flags: number;
}

export interface HostChemSpectrumReferrer {
  noteRelPath: string;
  noteTitle: string;
  attachmentRelPath: string;
  domainObjectKey: string;
  selectorKind: number;
  selectorSerialized: string;
  previewText: string;
  targetBasisRevision: string;
  state: number;
  flags: number;
}

export interface HostRebuildStatus {
  runMode: "dev" | "packaged";
  adapterAttached: boolean;
  status: {
    inFlight: boolean;
    hasLastResult: boolean;
    currentGeneration: number;
    lastCompletedGeneration: number;
    currentStartedAtNs: number;
    lastResultCode: string | null;
    lastResultAtNs: number;
    indexState: string;
  };
}

function getHostShell() {
  return (window as any).hostShell;
}

export async function getBootstrapInfo() {
  return getHostShell()?.bootstrap?.getInfo?.("nexus-bootstrap") as Promise<HostEnvelope<HostBootstrapInfo>>;
}

export async function getRuntimeSummary() {
  return getHostShell()?.runtime?.getSummary?.("nexus-runtime") as Promise<HostEnvelope<HostRuntimeSummary>>;
}

export async function getSessionStatus() {
  return getHostShell()?.session?.getStatus?.("nexus-session") as Promise<HostEnvelope<HostSessionStatus>>;
}

export async function openVault(vaultPath: string) {
  return getHostShell()?.session?.openVault?.(vaultPath, "nexus-open-vault") as Promise<HostEnvelope<any>>;
}

export async function closeVault() {
  return getHostShell()?.session?.closeVault?.("nexus-close-vault") as Promise<HostEnvelope<any>>;
}

export async function listEntries(parentRelPath = "") {
  return getHostShell()?.files?.listEntries?.({
    parentRelPath,
    limit: 256
  }, `nexus-files-${parentRelPath || "root"}`) as Promise<HostEnvelope<{ parentRelPath: string; count: number; items: HostEntry[] }>>;
}

export async function listRecentNotes() {
  return getHostShell()?.files?.listRecent?.({
    limit: 24
  }, "nexus-files-recent") as Promise<HostEnvelope<{ count: number; items: HostEntry[] }>>;
}

export async function readNote(relPath: string) {
  return getHostShell()?.files?.readNote?.({
    relPath
  }, `nexus-read-${relPath}`) as Promise<HostEnvelope<HostNote>>;
}

export async function querySearch(query: string) {
  return getHostShell()?.search?.query?.({
    query,
    limit: 24
  }, `nexus-search-${query}`) as Promise<HostEnvelope<{ totalHits: number; items: any[] }>>;
}

export async function listAttachments(limit = 64) {
  return getHostShell()?.attachments?.list?.({
    limit
  }, "nexus-attachments-list") as Promise<HostEnvelope<{ count: number; items: HostAttachmentRecord[] }>>;
}

export async function getAttachment(attachmentRelPath: string) {
  return getHostShell()?.attachments?.get?.({
    attachmentRelPath
  }, `nexus-attachment-${attachmentRelPath}`) as Promise<HostEnvelope<HostAttachmentRecord>>;
}

export async function queryNoteAttachmentRefs(noteRelPath: string, limit = 64) {
  return getHostShell()?.attachments?.queryNoteRefs?.({
    noteRelPath,
    limit
  }, `nexus-attachments-note-refs-${noteRelPath}`) as Promise<HostEnvelope<{ count: number; items: HostAttachmentRecord[] }>>;
}

export async function queryAttachmentReferrers(attachmentRelPath: string, limit = 64) {
  return getHostShell()?.attachments?.queryReferrers?.({
    attachmentRelPath,
    limit
  }, `nexus-attachment-referrers-${attachmentRelPath}`) as Promise<HostEnvelope<{ count: number; items: HostAttachmentReferrer[] }>>;
}

export async function getPdfMetadata(attachmentRelPath: string) {
  return getHostShell()?.pdf?.getMetadata?.({
    attachmentRelPath
  }, `nexus-pdf-metadata-${attachmentRelPath}`) as Promise<HostEnvelope<HostPdfMetadata>>;
}

export async function queryNotePdfSourceRefs(noteRelPath: string, limit = 64) {
  return getHostShell()?.pdf?.queryNoteSourceRefs?.({
    noteRelPath,
    limit
  }, `nexus-pdf-note-refs-${noteRelPath}`) as Promise<HostEnvelope<{ count: number; items: HostPdfSourceRef[] }>>;
}

export async function queryPdfReferrers(attachmentRelPath: string, limit = 64) {
  return getHostShell()?.pdf?.queryReferrers?.({
    attachmentRelPath,
    limit
  }, `nexus-pdf-referrers-${attachmentRelPath}`) as Promise<HostEnvelope<{ count: number; items: HostPdfReferrer[] }>>;
}

export async function queryChemistryMetadata(attachmentRelPath: string, limit = 64) {
  return getHostShell()?.chemistry?.queryMetadata?.({
    attachmentRelPath,
    limit
  }, `nexus-chem-meta-${attachmentRelPath}`) as Promise<HostEnvelope<{ count: number; items: HostDomainMetadataEntry[] }>>;
}

export async function listChemistrySpectra(limit = 64) {
  return getHostShell()?.chemistry?.listSpectra?.({
    limit
  }, "nexus-chem-list") as Promise<HostEnvelope<{ count: number; items: HostChemSpectrumRecord[] }>>;
}

export async function getChemistrySpectrum(attachmentRelPath: string) {
  return getHostShell()?.chemistry?.getSpectrum?.({
    attachmentRelPath
  }, `nexus-chem-spectrum-${attachmentRelPath}`) as Promise<HostEnvelope<HostChemSpectrumRecord>>;
}

export async function queryNoteChemistryRefs(noteRelPath: string, limit = 64) {
  return getHostShell()?.chemistry?.queryNoteRefs?.({
    noteRelPath,
    limit
  }, `nexus-chem-note-refs-${noteRelPath}`) as Promise<HostEnvelope<{ count: number; items: HostChemSpectrumSourceRef[] }>>;
}

export async function queryChemistryReferrers(attachmentRelPath: string, limit = 64) {
  return getHostShell()?.chemistry?.queryReferrers?.({
    attachmentRelPath,
    limit
  }, `nexus-chem-referrers-${attachmentRelPath}`) as Promise<HostEnvelope<{ count: number; items: HostChemSpectrumReferrer[] }>>;
}

export async function exportSupportBundle(outputPath: string) {
  return getHostShell()?.diagnostics?.exportSupportBundle?.({
    outputPath
  }, `nexus-diagnostics-export-${Date.now()}`) as Promise<HostEnvelope<{ outputPath: string; result: string }>>;
}

export async function getRebuildStatus() {
  return getHostShell()?.rebuild?.getStatus?.("nexus-rebuild-status") as Promise<HostEnvelope<HostRebuildStatus>>;
}

export async function startRebuild() {
  return getHostShell()?.rebuild?.start?.("nexus-rebuild-start") as Promise<HostEnvelope<{ result: string }>>;
}

export async function waitForRebuild(timeoutMs = 15000) {
  return getHostShell()?.rebuild?.wait?.({
    timeoutMs
  }, `nexus-rebuild-wait-${timeoutMs}`) as Promise<HostEnvelope<{ timeoutMs: number; result: string }>>;
}
