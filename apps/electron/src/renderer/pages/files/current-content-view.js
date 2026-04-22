/**
 * current-content-view.js
 *
 * Host-backed current content stage for the Files workspace surface.
 */

import { createCurrentNoteView } from "./current-note-view.js";

export function createCurrentContentView(opts = {}) {
  const {
    vaultName = "workspace",
    vaultPath = "",
    runtimeLine = "session=unknown · index=unknown",
    filesSurfaceState = null
  } = opts;
  const selectedEntry = filesSurfaceState?.selectedEntry ?? null;
  const currentNoteEnvelope = filesSurfaceState?.currentNoteEnvelope ?? null;

  const stage = document.createElement("section");
  stage.className = "files-current-content";
  stage.style.cssText = `
    min-height: 560px;
    border-radius: 0 0 24px 24px;
    background:
      radial-gradient(circle at top center, rgba(139,92,246,0.06), transparent 28%),
      linear-gradient(180deg, rgba(26,25,31,0.98), rgba(19,18,24,0.98));
  `;

  if (filesSurfaceState?.loading) {
    stage.appendChild(createStatusState("Loading vault content", "正在拉取 Files 根目录、Recent Notes 和当前内容对象。"));
  } else if (filesSurfaceState?.loadingSelection && selectedEntry) {
    stage.appendChild(createStatusState("Opening content", `正在读取 ${selectedEntry.relPath || selectedEntry.name || "selected entry"}。`));
  } else if (currentNoteEnvelope?.ok) {
    stage.appendChild(createCurrentNoteView({
      vaultName,
      noteRecord: currentNoteEnvelope.data,
      runtimeLine
    }));
  } else if (selectedEntry && !isNoteLikeSelection(selectedEntry)) {
    stage.appendChild(createSelectionSummary(selectedEntry));
  } else if (selectedEntry && currentNoteEnvelope?.ok === false) {
    stage.appendChild(createStatusState(
      currentNoteEnvelope.error?.code || "Read failed",
      currentNoteEnvelope.error?.message || "Failed to read the selected note."
    ));
  } else if (!selectedEntry) {
    stage.appendChild(createStatusState("No content selected", "当前 vault 还没有可读取的 note，或者当前选择不是 note。"));
  }

  stage.appendChild(createFooterBar(vaultPath, runtimeLine, selectedEntry, currentNoteEnvelope));
  return stage;
}

function createFooterBar(vaultPath, runtimeLine, selectedEntry, currentNoteEnvelope) {
  const footer = document.createElement("div");
  footer.style.cssText = `
    display: grid;
    grid-template-columns: repeat(auto-fit, minmax(220px, 1fr));
    gap: 12px;
    padding: 0 30px 30px;
  `;

  footer.appendChild(createFactCard(
    "Current item",
    selectedEntry?.relPath || "No current selection",
    selectedEntry ? `kind=${selectedEntry.kind || "entry"}` : "Files content selection is empty."
  ));
  footer.appendChild(createFactCard("Vault path", vaultPath || "No active vault path", "Sourced from the current session envelope."));
  footer.appendChild(createFactCard(
    "Runtime",
    runtimeLine,
    currentNoteEnvelope?.ok ? "Current note body is now sourced through window.hostShell.files.readNote()." : "Current view falls back to a stable state when no note body is available."
  ));
  return footer;
}

function createFactCard(label, title, body) {
  const card = document.createElement("div");
  card.style.cssText = `
    padding: 14px;
    border-radius: 16px;
    border: 1px solid rgba(255,255,255,0.06);
    background: rgba(255,255,255,0.03);
  `;

  const labelEl = document.createElement("div");
  labelEl.style.cssText = "font-size:11px; letter-spacing:0.16em; text-transform:uppercase; color:#8b83a2; margin-bottom:8px;";
  labelEl.textContent = label;
  card.appendChild(labelEl);

  const titleEl = document.createElement("div");
  titleEl.style.cssText = "font-size:14px; font-weight:600; color:#f5f3ff; word-break:break-word;";
  titleEl.textContent = title;
  card.appendChild(titleEl);

  const bodyEl = document.createElement("div");
  bodyEl.style.cssText = "margin-top:6px; font-size:12px; line-height:1.7; color:#978fae; word-break:break-word;";
  bodyEl.textContent = body;
  card.appendChild(bodyEl);

  return card;
}

function createStatusState(title, body) {
  const wrapper = document.createElement("div");
  wrapper.style.cssText = `
    max-width: 820px;
    margin: 0 auto;
    padding: 48px 30px 40px;
  `;

  const card = document.createElement("div");
  card.style.cssText = `
    padding: 24px 24px 22px;
    border-radius: 22px;
    border: 1px solid rgba(255,255,255,0.06);
    background: rgba(255,255,255,0.03);
  `;
  card.innerHTML = `
    <div style="font-size:12px; letter-spacing:0.18em; text-transform:uppercase; color:#a89ec6; margin-bottom:14px;">Files</div>
    <div style="font-size:30px; font-weight:700; letter-spacing:-0.03em; color:#faf7ff;">${escapeHtml(title)}</div>
    <div style="margin-top:12px; font-size:15px; line-height:1.9; color:#ddd6f4;">${escapeHtml(body)}</div>
  `;
  wrapper.appendChild(card);
  return wrapper;
}

function createSelectionSummary(entry) {
  const title = entry?.title || entry?.name || entry?.relPath || "Selected entry";
  const body = entry?.isDirectory || entry?.kind === "directory"
    ? "当前选择是文件夹。Files baseline 已能列出它，但当前主区仍只渲染 note body，不会在前端层伪造文件树内容。"
    : "当前选择不是 Markdown note。Files baseline 已能把它作为内容对象列出来，但当前主区不会把 attachment 假装成 note。";

  return createStatusState(title, body);
}

function isNoteLikeSelection(entry) {
  return entry?.kind === "note" || (typeof entry?.relPath === "string" && entry.relPath.toLowerCase().endsWith(".md"));
}

function escapeHtml(text) {
  return String(text)
    .replaceAll("&", "&amp;")
    .replaceAll("<", "&lt;")
    .replaceAll(">", "&gt;");
}
