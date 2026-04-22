/**
 * chemistry-page.js
 *
 * Top-level chemistry surface with list and detail tabs.
 */

import { chemistry } from "../../services/host-api-client.js";
import { createSpectrumList } from "./spectrum-list.js";
import { createSpectrumDetail } from "./spectrum-detail.js";
import { mapSpectrumList } from "./spectrum-view-model.js";
import { createStateSurface } from "../../components/shared/state-surface.js";

export function createChemistryPage() {
  const page = document.createElement("div");
  page.className = "chemistry-page";

  let currentRelPath = null;

  const tabs = document.createElement("div");
  tabs.style.cssText = `
    display: flex;
    gap: 8px;
    margin-bottom: 16px;
  `;

  const listTabBtn = document.createElement("button");
  listTabBtn.textContent = "谱图库";
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
    const detail = createSpectrumDetail(relPath);
    detail.backButton.addEventListener("click", showList);
    content.appendChild(detail.element);
  }

  async function loadList() {
    content.innerHTML = "";
    content.appendChild(createStateSurface("loading", { loadingLabel: "Loading spectra..." }));

    const env = await chemistry.listSpectra({ limit: 64 }, "app-chem-list");
    content.innerHTML = "";

    if (!env.ok) {
      if (env.error?.code === "HOST_KERNEL_SURFACE_NOT_INTEGRATED") {
        content.appendChild(createStateSurface("empty", {
          emptyMessage: "Chemistry features are not available in this build."
        }));
        return;
      }
      content.appendChild(createStateSurface("error", { error: env.error, onRetry: loadList }));
      return;
    }

    const viewModel = mapSpectrumList(env.data);
    const list = createSpectrumList(viewModel, {
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
