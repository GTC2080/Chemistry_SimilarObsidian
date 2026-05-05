// Reason: Own full AI embedding refresh-job preparation away from cache CRUD ABI.

#include "kernel/c_api.h"

#include "core/kernel_ai_embedding_cache_shared.h"
#include "core/kernel_ai_embedding_refresh_jobs_shared.h"
#include "core/kernel_internal.h"
#include "core/kernel_shared.h"
#include "storage/storage.h"

#include <algorithm>
#include <cctype>
#include <mutex>
#include <set>
#include <string>
#include <string_view>
#include <vector>

namespace {

std::string trim_ignored_root(std::string_view value) {
  std::size_t start = 0;
  while (start < value.size() && std::isspace(static_cast<unsigned char>(value[start])) != 0) {
    ++start;
  }
  while (start < value.size() && (value[start] == '/' || value[start] == '\\')) {
    ++start;
  }

  std::size_t end = value.size();
  while (end > start && std::isspace(static_cast<unsigned char>(value[end - 1])) != 0) {
    --end;
  }
  while (end > start && (value[end - 1] == '/' || value[end - 1] == '\\')) {
    --end;
  }

  return std::string(value.substr(start, end - start));
}

std::set<std::string> parse_ignored_roots(const char* ignored_roots_csv) {
  std::set<std::string> ignored;
  if (ignored_roots_csv == nullptr || ignored_roots_csv[0] == '\0') {
    return ignored;
  }

  const std::string_view raw(ignored_roots_csv);
  std::size_t start = 0;
  while (start <= raw.size()) {
    const std::size_t next = raw.find(',', start);
    const std::string root = trim_ignored_root(
        next == std::string_view::npos ? raw.substr(start) : raw.substr(start, next - start));
    if (!root.empty()) {
      ignored.insert(root);
    }
    if (next == std::string_view::npos) {
      break;
    }
    start = next + 1;
  }
  return ignored;
}

std::string first_rel_path_segment(const std::string& rel_path) {
  const std::size_t slash = rel_path.find('/');
  return slash == std::string::npos ? rel_path : rel_path.substr(0, slash);
}

void filter_ignored_roots(
    std::vector<kernel::storage::NoteCatalogRecord>& records,
    const std::set<std::string>& ignored_roots) {
  if (ignored_roots.empty()) {
    return;
  }

  records.erase(
      std::remove_if(
          records.begin(),
          records.end(),
          [&](const kernel::storage::NoteCatalogRecord& record) {
            return ignored_roots.contains(first_rel_path_segment(record.rel_path));
          }),
      records.end());
}

}  // namespace

extern "C" kernel_status kernel_prepare_ai_embedding_refresh_jobs(
    kernel_handle* handle,
    const char* ignored_roots_csv,
    const std::size_t limit,
    const std::uint8_t force_refresh,
    kernel_ai_embedding_refresh_job_list* out_jobs) {
  kernel::core::ai::reset_ai_embedding_refresh_job_list(out_jobs);
  if (handle == nullptr || out_jobs == nullptr || limit == 0) {
    return kernel::core::make_status(KERNEL_ERROR_INVALID_ARGUMENT);
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

  filter_ignored_roots(records, parse_ignored_roots(ignored_roots_csv));

  std::vector<kernel::core::ai::AiEmbeddingRefreshJobRecord> jobs;
  const kernel_status status =
      kernel::core::ai::prepare_ai_embedding_refresh_job_records(
          handle,
          records,
          kernel::core::ai::ai_embedding_timestamp_map(timestamp_records),
          force_refresh != 0,
          jobs);
  if (status.code != KERNEL_OK) {
    return status;
  }

  return kernel::core::ai::fill_ai_embedding_refresh_job_list(jobs, out_jobs);
}
