// Reason: This file owns note and page search result marshalling.

#include "core/kernel_search_api_shared.h"

#include "core/kernel_shared.h"

#include <new>
#include <vector>

namespace kernel::core::search_api {

kernel_status fill_note_list_results(
    const std::vector<kernel::storage::NoteListHit>& hits,
    kernel_search_results* out_results) {
  if (hits.empty()) {
    return kernel::core::make_status(KERNEL_OK);
  }

  auto* owned_hits = new (std::nothrow) kernel_search_hit[hits.size()];
  if (owned_hits == nullptr) {
    return kernel::core::make_status(KERNEL_ERROR_INTERNAL);
  }

  for (size_t index = 0; index < hits.size(); ++index) {
    owned_hits[index].rel_path = nullptr;
    owned_hits[index].title = nullptr;
  }

  out_results->hits = owned_hits;
  out_results->count = hits.size();

  for (size_t index = 0; index < hits.size(); ++index) {
    out_results->hits[index].rel_path = kernel::core::duplicate_c_string(hits[index].rel_path);
    out_results->hits[index].title = kernel::core::duplicate_c_string(hits[index].title);
    out_results->hits[index].match_flags = KERNEL_SEARCH_MATCH_NONE;
    if (out_results->hits[index].rel_path == nullptr ||
        out_results->hits[index].title == nullptr) {
      kernel::core::free_search_results_impl(out_results);
      return kernel::core::make_status(KERNEL_ERROR_INTERNAL);
    }
  }

  return kernel::core::make_status(KERNEL_OK);
}

kernel_status fill_search_results(
    const std::vector<kernel::search::SearchHit>& hits,
    kernel_search_results* out_results) {
  if (hits.empty()) {
    return kernel::core::make_status(KERNEL_OK);
  }

  auto* owned_hits = new (std::nothrow) kernel_search_hit[hits.size()];
  if (owned_hits == nullptr) {
    return kernel::core::make_status(KERNEL_ERROR_INTERNAL);
  }

  for (size_t index = 0; index < hits.size(); ++index) {
    owned_hits[index].rel_path = nullptr;
    owned_hits[index].title = nullptr;
    owned_hits[index].match_flags = KERNEL_SEARCH_MATCH_NONE;
  }

  out_results->hits = owned_hits;
  out_results->count = hits.size();

  for (size_t index = 0; index < hits.size(); ++index) {
    out_results->hits[index].rel_path = kernel::core::duplicate_c_string(hits[index].rel_path);
    out_results->hits[index].title = kernel::core::duplicate_c_string(hits[index].title);
    out_results->hits[index].match_flags = hits[index].match_flags;
    if (out_results->hits[index].rel_path == nullptr ||
        out_results->hits[index].title == nullptr) {
      kernel::core::free_search_results_impl(out_results);
      return kernel::core::make_status(KERNEL_ERROR_INTERNAL);
    }
  }

  return kernel::core::make_status(KERNEL_OK);
}

kernel_status fill_search_page_results(
    const kernel::search::SearchPage& page,
    kernel_search_page* out_page) {
  out_page->total_hits = page.total_hits;
  out_page->has_more = page.has_more ? 1 : 0;
  if (page.hits.empty()) {
    return kernel::core::make_status(KERNEL_OK);
  }

  auto* owned_hits = new (std::nothrow) kernel_search_page_hit[page.hits.size()];
  if (owned_hits == nullptr) {
    out_page->total_hits = 0;
    out_page->has_more = 0;
    return kernel::core::make_status(KERNEL_ERROR_INTERNAL);
  }

  for (size_t index = 0; index < page.hits.size(); ++index) {
    owned_hits[index].rel_path = nullptr;
    owned_hits[index].title = nullptr;
    owned_hits[index].snippet = nullptr;
    owned_hits[index].match_flags = KERNEL_SEARCH_MATCH_NONE;
    owned_hits[index].snippet_status = KERNEL_SEARCH_SNIPPET_NONE;
    owned_hits[index].result_kind = KERNEL_SEARCH_RESULT_NOTE;
    owned_hits[index].result_flags = KERNEL_SEARCH_RESULT_FLAG_NONE;
    owned_hits[index].score = 0.0;
  }

  out_page->hits = owned_hits;
  out_page->count = page.hits.size();

  for (size_t index = 0; index < page.hits.size(); ++index) {
    out_page->hits[index].rel_path =
        kernel::core::duplicate_c_string(page.hits[index].rel_path);
    out_page->hits[index].title =
        kernel::core::duplicate_c_string(page.hits[index].title);
    out_page->hits[index].snippet =
        kernel::core::duplicate_c_string(page.hits[index].snippet);
    out_page->hits[index].match_flags = page.hits[index].match_flags;
    out_page->hits[index].snippet_status = page.hits[index].snippet_status;
    out_page->hits[index].result_kind = page.hits[index].result_kind;
    out_page->hits[index].result_flags = page.hits[index].result_flags;
    out_page->hits[index].score = page.hits[index].score;
    if (out_page->hits[index].rel_path == nullptr ||
        out_page->hits[index].title == nullptr ||
        out_page->hits[index].snippet == nullptr) {
      free_search_page_impl(out_page);
      return kernel::core::make_status(KERNEL_ERROR_INTERNAL);
    }
  }

  return kernel::core::make_status(KERNEL_OK);
}

}  // namespace kernel::core::search_api
