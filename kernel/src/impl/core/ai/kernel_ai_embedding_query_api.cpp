// Reason: Expose AI embedding query and changed-note RAG ABI separately from cache mutation flows.

#include "kernel/c_api.h"

#include "core/kernel_ai_embedding_cache_shared.h"
#include "core/kernel_internal.h"
#include "core/kernel_shared.h"
#include "storage/storage.h"

#include <mutex>
#include <string>
#include <vector>

extern "C" kernel_status kernel_build_ai_rag_context_from_changed_note_paths(
    kernel_handle* handle,
    const char* changed_paths_lf,
    kernel_owned_buffer* out_buffer) {
  if (out_buffer != nullptr) {
    *out_buffer = kernel_owned_buffer{};
  }
  if (handle == nullptr || out_buffer == nullptr) {
    return kernel::core::make_status(KERNEL_ERROR_INVALID_ARGUMENT);
  }

  kernel_path_list filtered_paths{};
  const kernel_status filter_status =
      kernel_filter_changed_markdown_paths(changed_paths_lf, &filtered_paths);
  if (filter_status.code != KERNEL_OK) {
    kernel_free_path_list(&filtered_paths);
    return filter_status;
  }
  const std::vector<std::string> rel_paths = kernel::core::ai::copy_path_list(filtered_paths);
  kernel_free_path_list(&filtered_paths);
  if (rel_paths.empty()) {
    return kernel_build_ai_rag_context_from_note_paths(
        nullptr,
        nullptr,
        nullptr,
        nullptr,
        0,
        out_buffer);
  }

  std::vector<std::string> readable_paths;
  std::vector<std::string> readable_contents;
  readable_paths.reserve(rel_paths.size());
  readable_contents.reserve(rel_paths.size());
  for (const auto& rel_path : rel_paths) {
    std::string content;
    const std::error_code read_ec =
        kernel::core::ai::read_note_content(handle, rel_path, content);
    if (read_ec) {
      continue;
    }
    readable_paths.push_back(rel_path);
    readable_contents.push_back(std::move(content));
  }

  std::vector<const char*> path_ptrs;
  std::vector<std::size_t> path_sizes;
  std::vector<const char*> content_ptrs;
  std::vector<std::size_t> content_sizes;
  path_ptrs.reserve(readable_paths.size());
  path_sizes.reserve(readable_paths.size());
  content_ptrs.reserve(readable_contents.size());
  content_sizes.reserve(readable_contents.size());
  for (std::size_t index = 0; index < readable_paths.size(); ++index) {
    path_ptrs.push_back(readable_paths[index].data());
    path_sizes.push_back(readable_paths[index].size());
    content_ptrs.push_back(readable_contents[index].data());
    content_sizes.push_back(readable_contents[index].size());
  }

  return kernel_build_ai_rag_context_from_note_paths(
      path_ptrs.data(),
      path_sizes.data(),
      content_ptrs.data(),
      content_sizes.data(),
      readable_paths.size(),
      out_buffer);
}

extern "C" kernel_status kernel_query_ai_embedding_top_notes(
    kernel_handle* handle,
    const float* query_values,
    const std::size_t query_value_count,
    const char* exclude_rel_path,
    const std::size_t limit,
    kernel_search_results* out_results) {
  kernel::core::ai::reset_search_results(out_results);
  std::string normalized_exclude_rel_path;
  if (handle == nullptr || query_values == nullptr || query_value_count == 0 || limit == 0 ||
      out_results == nullptr ||
      !kernel::core::ai::normalize_optional_exclude_rel_path(
          exclude_rel_path,
          normalized_exclude_rel_path)) {
    return kernel::core::make_status(KERNEL_ERROR_INVALID_ARGUMENT);
  }

  std::vector<kernel::storage::NoteListHit> hits;
  {
    std::lock_guard lock(handle->storage_mutex);
    const std::error_code ec =
        kernel::storage::list_top_ai_embedding_notes(
            handle->storage,
            query_values,
            query_value_count,
            normalized_exclude_rel_path,
            limit,
            hits);
    if (ec) {
      return kernel::core::make_status(kernel::core::map_error(ec));
    }
  }

  return kernel::core::ai::fill_search_results(hits, out_results);
}
