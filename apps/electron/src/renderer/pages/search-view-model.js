/**
 * search-view-model.js
 *
 * Mapper from search.query envelope to renderer-friendly view model.
 * Validates required fields and flags host gaps.
 */

import { reportHostGap } from "../services/envelope-guard.js";

const REQUIRED_ITEM_FIELDS = ["title", "rel_path", "snippet", "kind"];
const OPTIONAL_ITEM_FIELDS = ["tags", "backlinks_count"];

/**
 * @param {any} envelopeData
 * @returns {{ items: Array<any>, total: number, has_more: boolean, offset: number, limit: number }}
 */
export function mapSearchPage(envelopeData) {
  if (!envelopeData || typeof envelopeData !== "object") {
    return { items: [], total: 0, has_more: false, offset: 0, limit: 25 };
  }

  const items = Array.isArray(envelopeData.items) ? envelopeData.items : [];
  const validatedItems = items.map((item, index) => {
    const mapped = { ...item };
    for (const field of REQUIRED_ITEM_FIELDS) {
      if (!(field in mapped)) {
        reportHostGap(
          "EXPLICIT-HOST-GAP-003",
          `Search result item[${index}] missing required field: ${field}`
        );
        mapped[field] = mapped[field] ?? "";
      }
    }
    for (const field of OPTIONAL_ITEM_FIELDS) {
      if (!(field in mapped)) {
        mapped[field] = null;
      }
    }
    return mapped;
  });

  const total = typeof envelopeData.total === "number" ? envelopeData.total : validatedItems.length;
  const hasMore = Boolean(envelopeData.has_more);
  const offset = typeof envelopeData.offset === "number" ? envelopeData.offset : 0;
  const limit = typeof envelopeData.limit === "number" ? envelopeData.limit : 25;

  if (!("total" in envelopeData) || !("has_more" in envelopeData) || !("offset" in envelopeData) || !("limit" in envelopeData)) {
    reportHostGap(
      "EXPLICIT-HOST-GAP-002",
      "Search response missing pagination metadata fields (total, has_more, offset, limit)."
    );
  }

  return {
    items: validatedItems,
    total,
    has_more: hasMore,
    offset,
    limit
  };
}
