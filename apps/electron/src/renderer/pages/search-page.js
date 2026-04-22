/**
 * search-page.js
 *
 * Top-level search surface.
 */

import { search } from "../services/host-api-client.js";
import { createStateSurface } from "../components/shared/state-surface.js";
import { createSearchFilterBar } from "./search-filter-bar.js";
import { createSearchResultList } from "./search-result-list.js";
import { mapSearchPage } from "./search-view-model.js";

export function createSearchPage() {
  const page = document.createElement("div");
  page.className = "search-page";

  let currentRequest = { query: "", kind: "all", sortMode: "rel_path_asc", limit: 25, offset: 0 };
  let lastEnvelope = null;
  let isLoading = false;

  const filterBar = createSearchFilterBar(currentRequest, {
    onSearch: (req) => {
      currentRequest = { ...req, offset: 0 };
      executeSearch();
    }
  });
  page.appendChild(filterBar);

  const resultsArea = document.createElement("div");
  resultsArea.className = "search-results-area";
  page.appendChild(resultsArea);

  async function executeSearch() {
    if (!currentRequest.query) {
      resultsArea.innerHTML = "";
      resultsArea.appendChild(createStateSurface("empty", { emptyMessage: "Enter a query to search." }));
      return;
    }

    isLoading = true;
    resultsArea.innerHTML = "";
    resultsArea.appendChild(createStateSurface("loading", { loadingLabel: "Searching..." }));

    const envelope = await search.query(currentRequest, "app-search");
    lastEnvelope = envelope;
    isLoading = false;

    resultsArea.innerHTML = "";

    if (!envelope.ok) {
      const code = envelope.error?.code ?? "";
      if (code === "HOST_SESSION_NOT_OPEN") {
        resultsArea.appendChild(createStateSurface("unavailable", {
          message: "Open a vault to search.",
          onRetry: executeSearch
        }));
      } else if (code === "HOST_KERNEL_ADAPTER_UNAVAILABLE") {
        resultsArea.appendChild(createStateSurface("unavailable", {
          message: "Host unavailable.",
          onRetry: executeSearch
        }));
      } else {
        resultsArea.appendChild(createStateSurface("error", {
          error: envelope.error,
          onRetry: executeSearch
        }));
      }
      return;
    }

    const viewModel = mapSearchPage(envelope.data);
    const list = createSearchResultList(viewModel, {
      onPageNext: () => {
        currentRequest.offset += currentRequest.limit;
        executeSearch();
      },
      onPagePrev: () => {
        currentRequest.offset = Math.max(0, currentRequest.offset - currentRequest.limit);
        executeSearch();
      }
    });
    resultsArea.appendChild(list);
  }

  // Initial empty state
  resultsArea.appendChild(createStateSurface("empty", { emptyMessage: "Enter a query to search." }));

  return page;
}
