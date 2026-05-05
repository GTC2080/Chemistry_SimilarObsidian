// Reason: Expose AI embedding cache mutation ABI separately from query/RAG flows.

#include "kernel/c_api.h"

#include "core/kernel_ai_embedding_cache_shared.h"
#include "core/kernel_internal.h"
#include "core/kernel_shared.h"
#include "storage/storage.h"

#include <cstdint>
#include <mutex>
#include <string>
#include <vector>

extern "C" kernel_status kernel_update_ai_embedding(
    kernel_handle* handle,
    const char* note_rel_path,
    const float* values,
    const std::size_t value_count) {
  std::string normalized_rel_path;
  if (handle == nullptr || values == nullptr || value_count == 0 ||
      !kernel::core::ai::normalize_note_rel_path(note_rel_path, normalized_rel_path)) {
    return kernel::core::make_status(KERNEL_ERROR_INVALID_ARGUMENT);
  }

  std::lock_guard lock(handle->storage_mutex);
  const std::error_code ec =
      kernel::storage::update_ai_embedding(
          handle->storage,
          normalized_rel_path,
          values,
          value_count);
  return kernel::core::make_status(kernel::core::map_error(ec));
}

extern "C" kernel_status kernel_clear_ai_embeddings(kernel_handle* handle) {
  if (handle == nullptr) {
    return kernel::core::make_status(KERNEL_ERROR_INVALID_ARGUMENT);
  }

  std::lock_guard lock(handle->storage_mutex);
  const std::error_code ec = kernel::storage::clear_ai_embeddings(handle->storage);
  return kernel::core::make_status(kernel::core::map_error(ec));
}

extern "C" kernel_status kernel_delete_ai_embedding_note(
    kernel_handle* handle,
    const char* note_rel_path,
    std::uint8_t* out_deleted) {
  std::string normalized_rel_path;
  if (handle == nullptr || out_deleted == nullptr ||
      !kernel::core::ai::normalize_note_rel_path(note_rel_path, normalized_rel_path)) {
    return kernel::core::make_status(KERNEL_ERROR_INVALID_ARGUMENT);
  }

  bool deleted = false;
  {
    std::lock_guard lock(handle->storage_mutex);
    const std::error_code ec =
        kernel::storage::delete_ai_embedding_note(
            handle->storage,
            normalized_rel_path,
            deleted);
    if (ec) {
      return kernel::core::make_status(kernel::core::map_error(ec));
    }
  }

  *out_deleted = deleted ? 1 : 0;
  return kernel::core::make_status(KERNEL_OK);
}

extern "C" kernel_status kernel_delete_changed_ai_embedding_notes(
    kernel_handle* handle,
    const char* changed_paths_lf,
    std::uint64_t* out_deleted_count) {
  if (out_deleted_count != nullptr) {
    *out_deleted_count = 0;
  }
  if (handle == nullptr || out_deleted_count == nullptr) {
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
    return kernel::core::make_status(KERNEL_OK);
  }

  std::uint64_t deleted_count = 0;
  {
    std::lock_guard lock(handle->storage_mutex);
    for (const auto& rel_path : rel_paths) {
      bool deleted = false;
      const std::error_code ec =
          kernel::storage::delete_ai_embedding_note(handle->storage, rel_path, deleted);
      if (ec) {
        return kernel::core::make_status(kernel::core::map_error(ec));
      }
      if (deleted) {
        ++deleted_count;
      }
    }
  }

  *out_deleted_count = deleted_count;
  return kernel::core::make_status(KERNEL_OK);
}

extern "C" void kernel_free_ai_embedding_refresh_job_list(
    kernel_ai_embedding_refresh_job_list* jobs) {
  kernel::core::ai::free_ai_embedding_refresh_job_list_impl(jobs);
}
