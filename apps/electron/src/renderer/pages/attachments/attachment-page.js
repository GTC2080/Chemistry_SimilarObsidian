/**
 * attachment-page.js
 *
 * Top-level attachment surface with list and detail tabs.
 */

import { attachments } from "../../services/host-api-client.js";
import { createAttachmentList } from "./attachment-list.js";
import { createAttachmentDetail } from "./attachment-detail.js";
import { mapAttachmentList } from "./attachment-view-model.js";
import { createStateSurface } from "../../components/shared/state-surface.js";

export function createAttachmentPage() {
  const page = document.createElement("div");
  page.className = "attachment-page";

  let currentRelPath = null;

  const tabs = document.createElement("div");
  tabs.style.cssText = `
    display: flex;
    gap: 8px;
    margin-bottom: 16px;
  `;

  const listTabBtn = document.createElement("button");
  listTabBtn.textContent = "列表";
  listTabBtn.style.cssText = tabButtonStyle(true);

  const detailTabBtn = document.createElement("button");
  detailTabBtn.textContent = "详情";
  detailTabBtn.style.cssText = tabButtonStyle(false);
  detailTabBtn.disabled = true;

  tabs.appendChild(listTabBtn);
  tabs.appendChild(detailTabBtn);
  page.appendChild(tabs);

  const content = document.createElement("div");
  page.appendChild(content);

  function showList() {
    listTabBtn.style.cssText = tabButtonStyle(true);
    detailTabBtn.style.cssText = tabButtonStyle(false);
    detailTabBtn.disabled = true;
    currentRelPath = null;
    loadList();
  }

  function showDetail(relPath) {
    currentRelPath = relPath;
    listTabBtn.style.cssText = tabButtonStyle(false);
    detailTabBtn.style.cssText = tabButtonStyle(true);
    detailTabBtn.disabled = false;

    content.innerHTML = "";
    const detail = createAttachmentDetail(relPath);
    detail.backButton.addEventListener("click", showList);
    content.appendChild(detail.element);
  }

  async function loadList() {
    content.innerHTML = "";
    content.appendChild(createStateSurface("loading", { loadingLabel: "Loading attachments..." }));

    const env = await attachments.list({ limit: 64 }, "app-attachments-list");
    content.innerHTML = "";

    if (!env.ok) {
      content.appendChild(createStateSurface("error", { error: env.error, onRetry: loadList }));
      return;
    }

    const viewModel = mapAttachmentList(env.data);
    const list = createAttachmentList(viewModel, {
      onSelect: (relPath) => showDetail(relPath)
    });
    content.appendChild(list);
  }

  listTabBtn.addEventListener("click", showList);

  showList();
  return page;
}

function tabButtonStyle(active) {
  const base = `
    padding: 10px 18px;
    border-radius: 12px;
    border: 1px solid rgba(255,255,255,0.08);
    background: rgba(255,255,255,0.04);
    cursor: pointer;
    font-size: 13px;
    color: #cabff7;
  `;
  if (active) {
    return base + " background: linear-gradient(180deg, rgba(124,58,237,0.28), rgba(124,58,237,0.12)); color: #fff; border-color: rgba(139,92,246,0.4);";
  }
  return base;
}
