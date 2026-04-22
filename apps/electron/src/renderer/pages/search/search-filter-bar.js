/**
 * search-filter-bar.js
 *
 * Filter controls for search.
 */

export function createSearchFilterBar(currentRequest, opts = {}) {
  const { onSearch } = opts;

  const bar = document.createElement("div");
  bar.className = "search-filter-bar";
  bar.style.cssText = `
    display: flex;
    flex-wrap: wrap;
    gap: 10px;
    align-items: center;
    padding: 14px;
    background: rgba(29, 28, 35, 0.94);
    border: 1px solid rgba(255,255,255,0.06);
    border-radius: 20px;
    margin-bottom: 16px;
  `;

  // Query input
  const queryInput = document.createElement("input");
  queryInput.type = "text";
  queryInput.placeholder = "输入关键词...";
  queryInput.value = currentRequest.query || "";
  queryInput.style.cssText = inputStyle();
  queryInput.style.flex = "1 1 240px";
  bar.appendChild(queryInput);

  // Kind select
  const kindSelect = document.createElement("select");
  kindSelect.style.cssText = selectStyle();
  const kindOptions = [
    { value: "all", label: "All" },
    { value: "note", label: "Notes" },
    { value: "attachment", label: "Attachments" }
  ];
  for (const opt of kindOptions) {
    const option = document.createElement("option");
    option.value = opt.value;
    option.textContent = opt.label;
    if (opt.value === (currentRequest.kind || "all")) option.selected = true;
    kindSelect.appendChild(option);
  }
  bar.appendChild(kindSelect);

  // Sort select
  const sortSelect = document.createElement("select");
  sortSelect.style.cssText = selectStyle();
  const sortOptions = [
    { value: "rel_path_asc", label: "Path A-Z" },
    { value: "rel_path_desc", label: "Path Z-A" }
  ];
  for (const opt of sortOptions) {
    const option = document.createElement("option");
    option.value = opt.value;
    option.textContent = opt.label;
    if (opt.value === (currentRequest.sortMode || "rel_path_asc")) option.selected = true;
    sortSelect.appendChild(option);
  }
  bar.appendChild(sortSelect);

  // Search button
  const searchBtn = document.createElement("button");
  searchBtn.textContent = "搜索";
  searchBtn.style.cssText = `
    padding: 10px 18px;
    border-radius: 12px;
    border: none;
    background: linear-gradient(180deg, #8b5cf6, #6d28d9);
    color: #fff;
    cursor: pointer;
    font-size: 13px;
    box-shadow: 0 14px 30px rgba(109,40,217,0.24);
  `;
  bar.appendChild(searchBtn);

  const submit = () => {
    onSearch?.({
      query: queryInput.value.trim(),
      kind: kindSelect.value,
      sortMode: sortSelect.value,
      limit: 25,
      offset: 0
    });
  };

  searchBtn.addEventListener("click", submit);
  queryInput.addEventListener("keydown", (e) => {
    if (e.key === "Enter") submit();
  });

  return bar;
}

function inputStyle() {
  return `
    padding: 10px 12px;
    border-radius: 12px;
    border: 1px solid rgba(255,255,255,0.08);
    font-size: 13px;
    background: rgba(255,255,255,0.04);
    color: #f5f3ff;
  `;
}

function selectStyle() {
  return `
    padding: 10px 12px;
    border-radius: 12px;
    border: 1px solid rgba(255,255,255,0.08);
    font-size: 13px;
    background: rgba(255,255,255,0.04);
    color: #f5f3ff;
  `;
}
