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
    padding: 10px 12px;
    background: #fff;
    border: 1px solid #e5e7eb;
    border-radius: 6px;
    margin-bottom: 12px;
  `;

  // Query input
  const queryInput = document.createElement("input");
  queryInput.type = "text";
  queryInput.placeholder = "Search...";
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
  searchBtn.textContent = "Search";
  searchBtn.style.cssText = `
    padding: 6px 16px;
    border-radius: 6px;
    border: none;
    background: #2563eb;
    color: #fff;
    cursor: pointer;
    font-size: 13px;
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
    padding: 6px 10px;
    border-radius: 6px;
    border: 1px solid #d1d5db;
    font-size: 13px;
  `;
}

function selectStyle() {
  return `
    padding: 6px 10px;
    border-radius: 6px;
    border: 1px solid #d1d5db;
    font-size: 13px;
    background: #fff;
  `;
}
