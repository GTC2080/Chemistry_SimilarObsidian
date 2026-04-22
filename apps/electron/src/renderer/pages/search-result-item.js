/**
 * search-result-item.js
 *
 * Single search result row.
 */

export function createSearchResultItem(item) {
  const el = document.createElement("div");
  el.className = "search-result-item";
  el.style.cssText = `
    padding: 12px 14px;
    border-radius: 6px;
    background: #fff;
    border: 1px solid #e5e7eb;
    margin-bottom: 8px;
    cursor: default;
  `;

  const title = document.createElement("div");
  title.style.cssText = "font-weight: 600; font-size: 14px; margin-bottom: 4px; color: #111827;";
  title.textContent = item.title || "Untitled";
  el.appendChild(title);

  if (item.snippet) {
    const snippet = document.createElement("div");
    snippet.style.cssText = "font-size: 13px; color: #4b5563; margin-bottom: 4px;";
    snippet.textContent = item.snippet;
    el.appendChild(snippet);
  }

  const meta = document.createElement("div");
  meta.style.cssText = "display: flex; gap: 10px; align-items: center; font-size: 12px; color: #6b7280;";

  const path = document.createElement("span");
  path.textContent = item.rel_path;
  meta.appendChild(path);

  const kind = document.createElement("span");
  kind.style.cssText = `
    padding: 1px 6px;
    border-radius: 4px;
    background: #f3f4f6;
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
