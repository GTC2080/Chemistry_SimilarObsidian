// Reason: This file owns backlink query C APIs.

#include "core/kernel_search_api_shared.h"

#include "core/kernel_internal.h"
#include "core/kernel_shared.h"

#include <mutex>
#include <string>
#include <vector>

extern "C" kernel_status kernel_query_backlinks(
    kernel_handle* handle,
    const char* rel_path,
    size_t limit,
    kernel_search_results* out_results) {
  kernel::core::search_api::reset_search_results(out_results);
  if (handle == nullptr || limit == 0 || out_results == nullptr ||
      !kernel::core::is_valid_relative_path(rel_path)) {
    return kernel::core::make_status(KERNEL_ERROR_INVALID_ARGUMENT);
  }

  const std::string normalized_rel_path = kernel::core::normalize_rel_path(rel_path);

  std::vector<kernel::storage::NoteListHit> hits;
  {
    std::lock_guard lock(handle->storage_mutex);
    const std::error_code ec =
        kernel::storage::list_backlinks_for_rel_path(
            handle->storage,
            normalized_rel_path,
            limit,
            hits);
    if (ec) {
      return kernel::core::make_status(kernel::core::map_error(ec));
    }
  }

  return kernel::core::search_api::fill_note_list_results(hits, out_results);
}
