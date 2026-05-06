// Reason: This file owns note search APIs and keeps page-query validation close
// to the C ABI entry points that need it.

#include "core/kernel_search_api_shared.h"

#include "core/kernel_internal.h"
#include "core/kernel_shared.h"

#include <mutex>
#include <string>
#include <string_view>
#include <vector>

extern "C" kernel_status kernel_search_notes(
    kernel_handle* handle,
    const char* query,
    kernel_search_results* out_results) {
  return kernel_search_notes_limited(handle, query, static_cast<size_t>(-1), out_results);
}

extern "C" kernel_status kernel_search_notes_limited(
    kernel_handle* handle,
    const char* query,
    size_t limit,
    kernel_search_results* out_results) {
  kernel::core::search_api::reset_search_results(out_results);
  if (handle == nullptr || kernel::core::is_null_or_empty(query) || out_results == nullptr ||
      limit == 0) {
    return kernel::core::make_status(KERNEL_ERROR_INVALID_ARGUMENT);
  }

  std::vector<kernel::search::SearchHit> hits;
  {
    std::lock_guard lock(handle->storage_mutex);
    const std::error_code search_ec =
        kernel::search::search_notes_compact(handle->storage, query, limit, hits);
    if (search_ec) {
      return kernel::core::make_status(kernel::core::map_error(search_ec));
    }
  }

  return kernel::core::search_api::fill_search_results(hits, out_results);
}

extern "C" kernel_status kernel_query_search(
    kernel_handle* handle,
    const kernel_search_query* request,
    kernel_search_page* out_page) {
  kernel::core::search_api::reset_search_page(out_page);
  if (handle == nullptr || request == nullptr || out_page == nullptr ||
      kernel::core::search_api::is_null_or_whitespace_only(request->query) ||
      request->limit == 0 ||
      request->limit > kernel::search::kSearchPageMaxLimit ||
      request->include_deleted != 0 ||
      (request->sort_mode != KERNEL_SEARCH_SORT_REL_PATH_ASC &&
       request->sort_mode != KERNEL_SEARCH_SORT_RANK_V1)) {
    return kernel::core::make_status(KERNEL_ERROR_INVALID_ARGUMENT);
  }

  if (request->kind != KERNEL_SEARCH_KIND_NOTE &&
      request->kind != KERNEL_SEARCH_KIND_ATTACHMENT &&
      request->kind != KERNEL_SEARCH_KIND_ALL) {
    return kernel::core::make_status(KERNEL_ERROR_INVALID_ARGUMENT);
  }

  if (!kernel::core::search_api::optional_search_field_uses_default(request->tag_filter) &&
      kernel::core::search_api::is_null_or_whitespace_only(request->tag_filter)) {
    return kernel::core::make_status(KERNEL_ERROR_INVALID_ARGUMENT);
  }
  if (request->kind == KERNEL_SEARCH_KIND_ATTACHMENT &&
      !kernel::core::search_api::optional_search_field_uses_default(request->tag_filter)) {
    return kernel::core::make_status(KERNEL_ERROR_INVALID_ARGUMENT);
  }
  if (request->kind == KERNEL_SEARCH_KIND_ATTACHMENT &&
      request->sort_mode == KERNEL_SEARCH_SORT_RANK_V1) {
    return kernel::core::make_status(KERNEL_ERROR_INVALID_ARGUMENT);
  }

  std::string normalized_path_prefix;
  if (!kernel::core::search_api::optional_search_field_uses_default(request->path_prefix)) {
    if (kernel::core::search_api::is_null_or_whitespace_only(request->path_prefix) ||
        !kernel::core::is_valid_relative_path(request->path_prefix)) {
      return kernel::core::make_status(KERNEL_ERROR_INVALID_ARGUMENT);
    }
    normalized_path_prefix = kernel::core::normalize_rel_path(request->path_prefix);
  }

  kernel::search::SearchPage page;
  {
    std::lock_guard lock(handle->storage_mutex);
    const std::error_code search_ec =
        kernel::search::search_page(
            handle->storage,
            kernel::search::SearchQuery{
                request->query,
                request->limit,
                request->offset,
                request->kind,
                kernel::core::search_api::optional_search_field_uses_default(request->tag_filter)
                    ? std::string_view{}
                    : std::string_view(request->tag_filter),
                normalized_path_prefix,
                false,
                request->sort_mode},
            page);
    if (search_ec) {
      return kernel::core::make_status(kernel::core::map_error(search_ec));
    }
  }

  return kernel::core::search_api::fill_search_page_results(page, out_page);
}
