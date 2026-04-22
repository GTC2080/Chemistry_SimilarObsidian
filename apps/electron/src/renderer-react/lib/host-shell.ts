export interface HostEnvelope<T = any> {
  ok: boolean;
  data: T | null;
  error: { code: string; message: string; details: any } | null;
  request_id?: string;
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

function getHostShell() {
  return (window as any).hostShell;
}

export async function getBootstrapInfo() {
  return getHostShell()?.bootstrap?.getInfo?.("nexus-bootstrap") as Promise<HostEnvelope<any>>;
}

export async function getRuntimeSummary() {
  return getHostShell()?.runtime?.getSummary?.("nexus-runtime") as Promise<HostEnvelope<any>>;
}

export async function getSessionStatus() {
  return getHostShell()?.session?.getStatus?.("nexus-session") as Promise<HostEnvelope<any>>;
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
