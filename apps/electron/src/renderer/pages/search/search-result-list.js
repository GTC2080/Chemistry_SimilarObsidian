/**
 * search-result-list.js
 *
 * List container with pagination.
 */

import { createSearchResultItem } from "./search-result-item.js";
import { createStateSurface } from "../../components/shared/state-surface.js";

export function createSearchResultList(viewModel, opts = {}) {
  const { onPageNext, onPagePrev } = opts;
  const container = document.createElement("div");
  container.className = "search-result-list";

  if (viewModel.items.length === 0) {
    container.appendChild(createStateSurface("empty", { emptyMessage: "No notes or attachments match your search." }));
    return container;
  }

  for (const item of viewModel.items) {
    container.appendChild(createSearchResultItem(item));
  }

  // Pagination footer
  const footer = document.createElement("div");
  footer.style.cssText = `
    display: flex;
    align-items: center;
    justify-content: space-between;
    margin-top: 12px;
    padding: 10px 0;
    font-size: 13px;
    color: #968ead;
  `;

  const info = document.createElement("span");
  const start = viewModel.offset + 1;
  const end = viewModel.offset + viewModel.items.length;
  info.textContent = `${start}-${end} of ${viewModel.total}`;
  footer.appendChild(info);

  const controls = document.createElement("div");
  controls.style.cssText = "display: flex; gap: 8px;";

  const prevBtn = document.createElement("button");
  prevBtn.textContent = "Previous";
  prevBtn.disabled = viewModel.offset <= 0;
  prevBtn.style.cssText = buttonStyle(prevBtn.disabled);
  prevBtn.addEventListener("click", () => onPagePrev?.());
  controls.appendChild(prevBtn);

  const nextBtn = document.createElement("button");
  nextBtn.textContent = "Next";
  nextBtn.disabled = !viewModel.has_more;
  nextBtn.style.cssText = buttonStyle(nextBtn.disabled);
  nextBtn.addEventListener("click", () => onPageNext?.());
  controls.appendChild(nextBtn);

  footer.appendChild(controls);
  container.appendChild(footer);

  return container;
}

function buttonStyle(disabled) {
  const base = `
    padding: 8px 14px;
    border-radius: 10px;
    border: 1px solid rgba(255,255,255,0.08);
    background: rgba(255,255,255,0.04);
    color: #efe8ff;
    cursor: pointer;
    font-size: 13px;
  `;
  if (disabled) {
    return base + " opacity: 0.5; cursor: not-allowed;";
  }
  return base;
}
