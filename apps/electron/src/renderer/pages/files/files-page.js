/**
 * files-page.js
 *
 * Host-backed Files workspace surface.
 */

import { createFilesContentHeader } from "./content-header.js";
import { createCurrentContentView } from "./current-content-view.js";
import { createRecentContentList } from "./recent-content-list.js";

export function createFilesPage(opts = {}) {
  const {
    vaultPath,
    runtimeEnvelope,
    filesSurfaceState = null,
    onSelectContent = null
  } = opts;
  const vaultName = baseName(vaultPath) || "workspace";
  const runtimeLine = buildRuntimeLine(runtimeEnvelope);
  const selectedEntry = filesSurfaceState?.selectedEntry ?? null;

  const page = document.createElement("div");
  page.className = "files-page";
  page.dataset.page = "files";
  page.style.cssText = `
    min-height: 100%;
    display: grid;
    grid-template-columns: minmax(300px, 340px) minmax(0, 1fr);
    gap: 18px;
    align-items: start;
  `;

  const recentPane = document.createElement("aside");
  recentPane.style.cssText = `
    min-width: 0;
    padding: 18px 16px;
    border-radius: 22px;
    border: 1px solid rgba(255,255,255,0.06);
    background: linear-gradient(180deg, rgba(32,31,38,0.96), rgba(24,23,29,0.96));
    box-shadow: inset 0 1px 0 rgba(255,255,255,0.04);
  `;

  recentPane.appendChild(createRecentContentList({
    entriesEnvelope: filesSurfaceState?.entriesEnvelope,
    recentEnvelope: filesSurfaceState?.recentEnvelope,
    selectedRelPath: filesSurfaceState?.selectedRelPath ?? null,
    loading: Boolean(filesSurfaceState?.loading),
    loadingSelection: Boolean(filesSurfaceState?.loadingSelection),
    onSelect: onSelectContent
  }));
  recentPane.appendChild(createVaultMetaCard({
    vaultName,
    vaultPath: vaultPath || "No active vault path",
    runtimeLine,
    selectedEntry
  }));
  page.appendChild(recentPane);

  const contentPane = document.createElement("section");
  contentPane.style.cssText = `
    min-width: 0;
    border-radius: 24px;
    border: 1px solid rgba(255,255,255,0.06);
    background: linear-gradient(180deg, rgba(28,27,34,0.98), rgba(20,19,25,0.98));
    overflow: hidden;
    box-shadow: inset 0 1px 0 rgba(255,255,255,0.04);
  `;

  contentPane.appendChild(createFilesContentHeader({
    title: selectedEntry?.title || "Files",
    subtitle: buildSubtitle(vaultName, selectedEntry, filesSurfaceState),
    runtimeLine
  }));
  contentPane.appendChild(createCurrentContentView({
    vaultName,
    vaultPath,
    runtimeLine,
    filesSurfaceState
  }));

  page.appendChild(contentPane);
  return page;
}

function createVaultMetaCard(opts = {}) {
  const {
    vaultName,
    vaultPath,
    runtimeLine,
    selectedEntry
  } = opts;

  const card = document.createElement("div");
  card.style.cssText = `
    margin-top: 18px;
    padding: 14px;
    border-radius: 18px;
    border: 1px solid rgba(255,255,255,0.06);
    background: rgba(255,255,255,0.03);
    display: grid;
    gap: 10px;
  `;

  card.appendChild(createMetaRow("Vault", vaultName));
  card.appendChild(createMetaRow("Path", vaultPath));
  card.appendChild(createMetaRow("Runtime", runtimeLine));
  card.appendChild(createMetaRow("Current", selectedEntry?.relPath || "No current selection"));
  return card;
}

function createMetaRow(label, value) {
  const wrapper = document.createElement("div");

  const labelEl = document.createElement("div");
  labelEl.style.cssText = "font-size:11px; letter-spacing:0.16em; text-transform:uppercase; color:#8b83a2; margin-bottom:6px;";
  labelEl.textContent = label;
  wrapper.appendChild(labelEl);

  const valueEl = document.createElement("div");
  valueEl.style.cssText = "font-size:12px; line-height:1.7; color:#ddd6fe; word-break:break-word;";
  valueEl.textContent = value;
  wrapper.appendChild(valueEl);
  return wrapper;
}

function buildSubtitle(vaultName, selectedEntry, filesSurfaceState) {
  if (filesSurfaceState?.loadingSelection) {
    return `${vaultName} · opening selection`;
  }

  if (!selectedEntry) {
    return `${vaultName} · no content selected`;
  }

  return `${vaultName} · ${selectedEntry.kind ?? "entry"} · ${selectedEntry.relPath ?? selectedEntry.name ?? "unknown"}`;
}

function buildRuntimeLine(runtimeEnvelope) {
  const runtime = runtimeEnvelope?.ok ? runtimeEnvelope.data : null;
  const state = runtime?.kernel_runtime?.index_state ?? "unknown";
  const session = runtime?.kernel_runtime?.session_state ?? "unknown";
  return `session=${session} · index=${state}`;
}

function baseName(vaultPath) {
  if (!vaultPath || typeof vaultPath !== "string") return "";
  const parts = vaultPath.split(/[\\/]/).filter(Boolean);
  return parts.length > 0 ? parts[parts.length - 1] : vaultPath;
}
