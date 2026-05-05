// Reason: Own changed-path AI embedding refresh-job preparation separately from full refresh scans.

#include "kernel/c_api.h"

#include "core/kernel_ai_embedding_cache_shared.h"
#include "core/kernel_ai_embedding_refresh_jobs_shared.h"
#include "core/kernel_internal.h"
#include "core/kernel_shared.h"
#include "index/refresh.h"
#include "storage/storage.h"

#include <cstdint>
#include <mutex>
#include <string>
#include <unordered_map>
#include <vector>

namespace {

void filter_records_by_rel_paths(
    std::vector<kernel::storage::NoteCatalogRecord>& records,
    const std::vector<std::string>& rel_paths) {
  std::unordered_map<std::string, kernel::storage::NoteCatalogRecord> by_rel_path;
  by_rel_path.reserve(records.size());
  for (auto& record : records) {
    by_rel_path.emplace(record.rel_path, std::move(record));
  }

  records.clear();
  records.reserve(rel_paths.size());
  for (const auto& rel_path : rel_paths) {
    auto found = by_rel_path.find(rel_path);
    if (found != by_rel_path.end()) {
      records.push_back(std::move(found->second));
    }
  }
}

}  // namespace

extern "C" kernel_status kernel_prepare_changed_ai_embedding_refresh_jobs(
    kernel_handle* handle,
    const char* changed_paths_lf,
    const std::size_t limit,
    kernel_ai_embedding_refresh_job_list* out_jobs) {
  kernel::core::ai::reset_ai_embedding_refresh_job_list(out_jobs);
  if (handle == nullptr || out_jobs == nullptr || limit == 0) {
    return kernel::core::make_status(KERNEL_ERROR_INVALID_ARGUMENT);
  }

  kernel_path_list filtered_paths{};
  const kernel_status filter_status =
      kernel_filter_changed_markdown_paths(changed_paths_lf, &filtered_paths);
  if (filter_status.code != KERNEL_OK) {
    return filter_status;
  }
  const std::vector<std::string> rel_paths = kernel::core::ai::copy_path_list(filtered_paths);
  kernel_free_path_list(&filtered_paths);
  if (rel_paths.empty()) {
    return kernel::core::make_status(KERNEL_OK);
  }

  {
    std::lock_guard lock(handle->storage_mutex);
    for (const auto& rel_path : rel_paths) {
      const std::error_code refresh_ec =
          kernel::index::refresh_markdown_path(handle->storage, handle->paths.root, rel_path);
      if (refresh_ec) {
        return kernel::core::make_status(kernel::core::map_error(refresh_ec));
      }
    }

    std::uint64_t indexed_note_count = 0;
    const std::error_code count_ec =
        kernel::storage::count_active_notes(handle->storage, indexed_note_count);
    if (count_ec) {
      return kernel::core::make_status(kernel::core::map_error(count_ec));
    }
    std::lock_guard runtime_lock(handle->runtime_mutex);
    handle->runtime.indexed_note_count = indexed_note_count;
  }

  std::vector<kernel::storage::NoteCatalogRecord> records;
  std::vector<kernel::storage::AiEmbeddingTimestampRecord> timestamp_records;
  {
    std::lock_guard lock(handle->storage_mutex);
    std::error_code ec =
        kernel::storage::list_note_catalog_records(handle->storage, limit, records);
    if (ec) {
      return kernel::core::make_status(kernel::core::map_error(ec));
    }
    ec = kernel::storage::list_ai_embedding_note_timestamps(handle->storage, timestamp_records);
    if (ec) {
      return kernel::core::make_status(kernel::core::map_error(ec));
    }
  }

  filter_records_by_rel_paths(records, rel_paths);

  std::vector<kernel::core::ai::AiEmbeddingRefreshJobRecord> jobs;
  const kernel_status status =
      kernel::core::ai::prepare_ai_embedding_refresh_job_records(
          handle,
          records,
          kernel::core::ai::ai_embedding_timestamp_map(timestamp_records),
          false,
          jobs);
  if (status.code != KERNEL_OK) {
    return status;
  }

  return kernel::core::ai::fill_ai_embedding_refresh_job_list(jobs, out_jobs);
}
