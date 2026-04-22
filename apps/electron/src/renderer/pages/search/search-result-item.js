/**
 * search-result-item.js
 *
 * Single search result row.
 */

export function createSearchResultItem(item) {
  const el = document.createElement("div");
  el.className = "search-result-item";
  el.style.cssText = `
    padding: 16px 18px;
    border-radius: 18px;
    background: rgba(29, 28, 35, 0.94);
    border: 1px solid rgba(255,255,255,0.06);
    margin-bottom: 10px;
    cursor: default;
  `;

  const title = document.createElement("div");
  title.style.cssText = "font-weight: 600; font-size: 15px; margin-bottom: 6px; color: #f5f3ff;";
  title.textContent = item.title || "Untitled";
  el.appendChild(title);

  if (item.snippet) {
    const snippet = document.createElement("div");
    snippet.style.cssText = "font-size: 13px; color: #c2bbd8; margin-bottom: 6px;";
    snippet.textContent = item.snippet;
    el.appendChild(snippet);
  }

  const meta = document.createElement("div");
  meta.style.cssText = "display: flex; flex-wrap: wrap; gap: 10px; align-items: center; font-size: 12px; color: #968ead;";

  const path = document.createElement("span");
  path.textContent = item.rel_path;
  meta.appendChild(path);

  const kind = document.createElement("span");
  kind.style.cssText = `
    padding: 2px 8px;
    border-radius: 999px;
    background: rgba(139,92,246,0.18);
    border: 1px solid rgba(255,255,255,0.06);
    color: #ddd6fe;
    text-transform: capitalize;
  `;
  kind.textContent = item.kind || "unknown";
  meta.appendChild(kind);

  if (Array.isArray(item.tags) && item.tags.length > 0) {
    const tags = document.createElement("span");
    tags.textContent = item.tags.join(", ");
    meta.appendChild(tags);
  }

  if (typeof item.backlinks_count === "number" && item.backlinks_count > 0) {
    const bl = document.createElement("span");
    bl.textContent = `${item.backlinks_count} backlink${item.backlinks_count === 1 ? "" : "s"}`;
    meta.appendChild(bl);
  }

  el.appendChild(meta);
  return el;
}
