// Reason: This file owns tag catalog and tag tree query C APIs.

#include "core/kernel_search_api_shared.h"

#include "core/kernel_internal.h"
#include "core/kernel_shared.h"

#include <mutex>
#include <vector>

extern "C" kernel_status kernel_query_tag_notes(
    kernel_handle* handle,
    const char* tag,
    size_t limit,
    kernel_search_results* out_results) {
  kernel::core::search_api::reset_search_results(out_results);
  if (handle == nullptr || kernel::core::search_api::is_null_or_whitespace_only(tag) ||
      limit == 0 || out_results == nullptr) {
    return kernel::core::make_status(KERNEL_ERROR_INVALID_ARGUMENT);
  }

  std::vector<kernel::storage::NoteListHit> hits;
  {
    std::lock_guard lock(handle->storage_mutex);
    const std::error_code ec = kernel::storage::list_notes_by_tag(handle->storage, tag, limit, hits);
    if (ec) {
      return kernel::core::make_status(kernel::core::map_error(ec));
    }
  }

  return kernel::core::search_api::fill_note_list_results(hits, out_results);
}

extern "C" kernel_status kernel_query_tags(
    kernel_handle* handle,
    size_t limit,
    kernel_tag_list* out_tags) {
  kernel::core::search_api::reset_tag_list(out_tags);
  if (handle == nullptr || limit == 0 || out_tags == nullptr) {
    return kernel::core::make_status(KERNEL_ERROR_INVALID_ARGUMENT);
  }

  std::vector<kernel::storage::TagSummaryRecord> records;
  {
    std::lock_guard lock(handle->storage_mutex);
    const std::error_code ec =
        kernel::storage::list_tag_summaries(handle->storage, limit, records);
    if (ec) {
      return kernel::core::make_status(kernel::core::map_error(ec));
    }
  }

  return kernel::core::search_api::fill_tag_list_results(records, out_tags);
}

extern "C" kernel_status kernel_query_tag_tree(
    kernel_handle* handle,
    size_t limit,
    kernel_tag_tree* out_tree) {
  kernel::core::search_api::reset_tag_tree(out_tree);
  if (handle == nullptr || limit == 0 || out_tree == nullptr) {
    return kernel::core::make_status(KERNEL_ERROR_INVALID_ARGUMENT);
  }

  std::vector<kernel::storage::TagSummaryRecord> records;
  {
    std::lock_guard lock(handle->storage_mutex);
    const std::error_code ec =
        kernel::storage::list_tag_summaries(handle->storage, limit, records);
    if (ec) {
      return kernel::core::make_status(kernel::core::map_error(ec));
    }
  }

  return kernel::core::search_api::fill_tag_tree_results(records, out_tree);
}
