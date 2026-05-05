// Reason: This file exposes the kernel-owned AI embedding cache ABI to host shells.

#include "kernel/c_api.h"

#include "core/kernel_internal.h"
#include "core/kernel_shared.h"
#include "index/refresh.h"
#include "platform/platform.h"
#include "storage/storage.h"

#include <algorithm>
#include <cctype>
#include <cstring>
#include <filesystem>
#include <mutex>
#include <new>
#include <set>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace {

void free_ai_embedding_timestamp_list_impl(kernel_ai_embedding_timestamp_list* timestamps) {
  if (timestamps == nullptr) {
    return;
  }

  if (timestamps->records != nullptr) {
    for (std::size_t index = 0; index < timestamps->count; ++index) {
      delete[] timestamps->records[index].rel_path;
      timestamps->records[index].rel_path = nullptr;
      timestamps->records[index].updated_at = 0;
    }
    delete[] timestamps->records;
  }

  timestamps->records = nullptr;
  timestamps->count = 0;
}

void reset_ai_embedding_timestamp_list(kernel_ai_embedding_timestamp_list* timestamps) {
  free_ai_embedding_timestamp_list_impl(timestamps);
}

void reset_search_results(kernel_search_results* out_results) {
  kernel::core::free_search_results_impl(out_results);
}

struct AiEmbeddingRefreshJobRecord {
  std::string rel_path;
  std::string title;
  std::string absolute_path;
  std::int64_t created_at = 0;
  std::int64_t updated_at = 0;
  std::string content;
};

void free_ai_embedding_refresh_job_list_impl(kernel_ai_embedding_refresh_job_list* jobs) {
  if (jobs == nullptr) {
    return;
  }

  if (jobs->jobs != nullptr) {
    for (std::size_t index = 0; index < jobs->count; ++index) {
      delete[] jobs->jobs[index].rel_path;
      delete[] jobs->jobs[index].title;
      delete[] jobs->jobs[index].absolute_path;
      delete[] jobs->jobs[index].content;
      jobs->jobs[index].rel_path = nullptr;
      jobs->jobs[index].title = nullptr;
      jobs->jobs[index].absolute_path = nullptr;
      jobs->jobs[index].content = nullptr;
      jobs->jobs[index].content_size = 0;
      jobs->jobs[index].created_at = 0;
      jobs->jobs[index].updated_at = 0;
    }
    delete[] jobs->jobs;
  }

  jobs->jobs = nullptr;
  jobs->count = 0;
}

void reset_ai_embedding_refresh_job_list(kernel_ai_embedding_refresh_job_list* jobs) {
  free_ai_embedding_refresh_job_list_impl(jobs);
}

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

std::int64_t note_updated_at_seconds(const kernel::storage::NoteCatalogRecord& record) {
  return static_cast<std::int64_t>(record.mtime_ns / 1000000000ULL);
}

std::unordered_map<std::string, std::int64_t> timestamp_map(
    const std::vector<kernel::storage::AiEmbeddingTimestampRecord>& records) {
  std::unordered_map<std::string, std::int64_t> timestamps;
  timestamps.reserve(records.size());
  for (const auto& record : records) {
    timestamps.emplace(record.rel_path, record.updated_at);
  }
  return timestamps;
}

kernel_status should_refresh_record(
    const kernel::storage::NoteCatalogRecord& record,
    const std::unordered_map<std::string, std::int64_t>& timestamps,
    const bool force_refresh,
    bool& out_should_refresh) {
  out_should_refresh = true;
  if (force_refresh) {
    return kernel::core::make_status(KERNEL_OK);
  }

  const auto found = timestamps.find(record.rel_path);
  std::uint8_t should_refresh = 0;
  const kernel_status status = kernel_should_refresh_ai_embedding_note(
      note_updated_at_seconds(record),
      found == timestamps.end() ? 0 : 1,
      found == timestamps.end() ? 0 : found->second,
      &should_refresh);
  if (status.code != KERNEL_OK) {
    return status;
  }
  out_should_refresh = should_refresh != 0;
  return kernel::core::make_status(KERNEL_OK);
}

kernel_status is_embedding_content_indexable(std::string_view content, bool& out_indexable) {
  std::uint8_t is_indexable = 0;
  const kernel_status status = kernel_is_ai_embedding_text_indexable(
      content.data(),
      content.size(),
      &is_indexable);
  if (status.code != KERNEL_OK) {
    return status;
  }
  out_indexable = is_indexable != 0;
  return kernel::core::make_status(KERNEL_OK);
}

std::error_code read_note_content(
    kernel_handle* handle,
    std::string_view rel_path,
    std::string& out_content) {
  kernel::platform::ReadFileResult file;
  const std::string rel_path_string(rel_path);
  const std::error_code ec =
      kernel::platform::read_file(kernel::core::resolve_note_path(handle, rel_path_string.c_str()), file);
  if (ec) {
    return ec;
  }
  out_content = std::move(file.bytes);
  return {};
}

std::filesystem::path absolute_note_path(kernel_handle* handle, std::string_view rel_path) {
  return (handle->paths.root / std::filesystem::path(std::string(rel_path))).lexically_normal();
}

kernel_status prepare_refresh_job_records(
    kernel_handle* handle,
    const std::vector<kernel::storage::NoteCatalogRecord>& records,
    const std::unordered_map<std::string, std::int64_t>& timestamps,
    const bool force_refresh,
    std::vector<AiEmbeddingRefreshJobRecord>& out_jobs) {
  out_jobs.clear();
  for (const auto& record : records) {
    bool should_refresh = true;
    const kernel_status refresh_status =
        should_refresh_record(record, timestamps, force_refresh, should_refresh);
    if (refresh_status.code != KERNEL_OK) {
      return refresh_status;
    }
    if (!should_refresh) {
      continue;
    }

    std::string content;
    const std::error_code read_ec = read_note_content(handle, record.rel_path, content);
    if (read_ec) {
      continue;
    }

    bool is_indexable = false;
    const kernel_status indexable_status = is_embedding_content_indexable(content, is_indexable);
    if (indexable_status.code != KERNEL_OK) {
      return indexable_status;
    }
    if (!is_indexable) {
      continue;
    }

    const std::int64_t updated_at = note_updated_at_seconds(record);
    out_jobs.push_back(AiEmbeddingRefreshJobRecord{
        record.rel_path,
        record.title,
        absolute_note_path(handle, record.rel_path).string(),
        updated_at,
        updated_at,
        std::move(content)});
  }

  for (const auto& job : out_jobs) {
    const kernel::storage::AiEmbeddingNoteMetadataRecord metadata{
        job.rel_path,
        job.title,
        job.absolute_path,
        job.created_at,
        job.updated_at};
    std::lock_guard lock(handle->storage_mutex);
    const std::error_code ec =
        kernel::storage::upsert_ai_embedding_note_metadata(handle->storage, metadata);
    if (ec) {
      return kernel::core::make_status(kernel::core::map_error(ec));
    }
  }

  return kernel::core::make_status(KERNEL_OK);
}

char* duplicate_bytes_as_c_string(std::string_view value) {
  auto* bytes = new (std::nothrow) char[value.size() + 1];
  if (bytes == nullptr) {
    return nullptr;
  }
  if (!value.empty()) {
    std::memcpy(bytes, value.data(), value.size());
  }
  bytes[value.size()] = '\0';
  return bytes;
}

bool normalize_note_rel_path(const char* rel_path, std::string& out_rel_path) {
  if (!kernel::core::is_valid_relative_path(rel_path)) {
    return false;
  }
  out_rel_path = kernel::core::normalize_rel_path(rel_path);
  return !out_rel_path.empty() && out_rel_path != ".";
}

bool normalize_optional_exclude_rel_path(const char* rel_path, std::string& out_rel_path) {
  out_rel_path.clear();
  if (rel_path == nullptr || rel_path[0] == '\0') {
    return true;
  }
  return normalize_note_rel_path(rel_path, out_rel_path);
}

bool metadata_to_record(
    const kernel_ai_embedding_note_metadata* metadata,
    kernel::storage::AiEmbeddingNoteMetadataRecord& out_record) {
  if (metadata == nullptr || kernel::core::is_null_or_empty(metadata->title) ||
      metadata->absolute_path == nullptr) {
    return false;
  }
  if (!normalize_note_rel_path(metadata->rel_path, out_record.rel_path)) {
    return false;
  }
  out_record.title = metadata->title;
  out_record.absolute_path = metadata->absolute_path;
  out_record.created_at = metadata->created_at;
  out_record.updated_at = metadata->updated_at;
  return true;
}

kernel_status fill_ai_embedding_timestamp_list(
    const std::vector<kernel::storage::AiEmbeddingTimestampRecord>& records,
    kernel_ai_embedding_timestamp_list* out_timestamps) {
  if (records.empty()) {
    return kernel::core::make_status(KERNEL_OK);
  }

  auto* owned_records =
      new (std::nothrow) kernel_ai_embedding_timestamp_record[records.size()];
  if (owned_records == nullptr) {
    return kernel::core::make_status(KERNEL_ERROR_INTERNAL);
  }

  for (std::size_t index = 0; index < records.size(); ++index) {
    owned_records[index].rel_path = nullptr;
    owned_records[index].updated_at = 0;
  }

  out_timestamps->records = owned_records;
  out_timestamps->count = records.size();

  for (std::size_t index = 0; index < records.size(); ++index) {
    out_timestamps->records[index].rel_path =
        kernel::core::duplicate_c_string(records[index].rel_path);
    out_timestamps->records[index].updated_at = records[index].updated_at;
    if (out_timestamps->records[index].rel_path == nullptr) {
      free_ai_embedding_timestamp_list_impl(out_timestamps);
      return kernel::core::make_status(KERNEL_ERROR_INTERNAL);
    }
  }

  return kernel::core::make_status(KERNEL_OK);
}

kernel_status fill_search_results(
    const std::vector<kernel::storage::NoteListHit>& hits,
    kernel_search_results* out_results) {
  if (hits.empty()) {
    return kernel::core::make_status(KERNEL_OK);
  }

  auto* owned_hits = new (std::nothrow) kernel_search_hit[hits.size()];
  if (owned_hits == nullptr) {
    return kernel::core::make_status(KERNEL_ERROR_INTERNAL);
  }

  for (std::size_t index = 0; index < hits.size(); ++index) {
    owned_hits[index].rel_path = nullptr;
    owned_hits[index].title = nullptr;
    owned_hits[index].match_flags = KERNEL_SEARCH_MATCH_NONE;
  }

  out_results->hits = owned_hits;
  out_results->count = hits.size();

  for (std::size_t index = 0; index < hits.size(); ++index) {
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

kernel_status fill_ai_embedding_refresh_job_list(
    const std::vector<AiEmbeddingRefreshJobRecord>& records,
    kernel_ai_embedding_refresh_job_list* out_jobs) {
  if (records.empty()) {
    return kernel::core::make_status(KERNEL_OK);
  }

  auto* owned_jobs = new (std::nothrow) kernel_ai_embedding_refresh_job[records.size()];
  if (owned_jobs == nullptr) {
    return kernel::core::make_status(KERNEL_ERROR_INTERNAL);
  }

  for (std::size_t index = 0; index < records.size(); ++index) {
    owned_jobs[index].rel_path = nullptr;
    owned_jobs[index].title = nullptr;
    owned_jobs[index].absolute_path = nullptr;
    owned_jobs[index].created_at = 0;
    owned_jobs[index].updated_at = 0;
    owned_jobs[index].content = nullptr;
    owned_jobs[index].content_size = 0;
  }

  out_jobs->jobs = owned_jobs;
  out_jobs->count = records.size();

  for (std::size_t index = 0; index < records.size(); ++index) {
    out_jobs->jobs[index].rel_path = kernel::core::duplicate_c_string(records[index].rel_path);
    out_jobs->jobs[index].title = kernel::core::duplicate_c_string(records[index].title);
    out_jobs->jobs[index].absolute_path =
        kernel::core::duplicate_c_string(records[index].absolute_path);
    out_jobs->jobs[index].created_at = records[index].created_at;
    out_jobs->jobs[index].updated_at = records[index].updated_at;
    out_jobs->jobs[index].content = duplicate_bytes_as_c_string(records[index].content);
    out_jobs->jobs[index].content_size = records[index].content.size();

    if (out_jobs->jobs[index].rel_path == nullptr || out_jobs->jobs[index].title == nullptr ||
        out_jobs->jobs[index].absolute_path == nullptr ||
        out_jobs->jobs[index].content == nullptr) {
      free_ai_embedding_refresh_job_list_impl(out_jobs);
      return kernel::core::make_status(KERNEL_ERROR_INTERNAL);
    }
  }

  return kernel::core::make_status(KERNEL_OK);
}

std::vector<std::string> copy_path_list(const kernel_path_list& paths) {
  std::vector<std::string> result;
  result.reserve(paths.count);
  for (std::size_t index = 0; index < paths.count; ++index) {
    if (paths.paths[index] != nullptr) {
      result.emplace_back(paths.paths[index]);
    }
  }
  return result;
}

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

extern "C" kernel_status kernel_upsert_ai_embedding_note_metadata(
    kernel_handle* handle,
    const kernel_ai_embedding_note_metadata* metadata) {
  kernel::storage::AiEmbeddingNoteMetadataRecord record{};
  if (handle == nullptr || !metadata_to_record(metadata, record)) {
    return kernel::core::make_status(KERNEL_ERROR_INVALID_ARGUMENT);
  }

  std::lock_guard lock(handle->storage_mutex);
  const std::error_code ec =
      kernel::storage::upsert_ai_embedding_note_metadata(handle->storage, record);
  return kernel::core::make_status(kernel::core::map_error(ec));
}

extern "C" kernel_status kernel_query_ai_embedding_note_timestamps(
    kernel_handle* handle,
    kernel_ai_embedding_timestamp_list* out_timestamps) {
  reset_ai_embedding_timestamp_list(out_timestamps);
  if (handle == nullptr || out_timestamps == nullptr) {
    return kernel::core::make_status(KERNEL_ERROR_INVALID_ARGUMENT);
  }

  std::vector<kernel::storage::AiEmbeddingTimestampRecord> records;
  {
    std::lock_guard lock(handle->storage_mutex);
    const std::error_code ec =
        kernel::storage::list_ai_embedding_note_timestamps(handle->storage, records);
    if (ec) {
      return kernel::core::make_status(kernel::core::map_error(ec));
    }
  }

  return fill_ai_embedding_timestamp_list(records, out_timestamps);
}

extern "C" kernel_status kernel_prepare_ai_embedding_refresh_jobs(
    kernel_handle* handle,
    const char* ignored_roots_csv,
    const std::size_t limit,
    const std::uint8_t force_refresh,
    kernel_ai_embedding_refresh_job_list* out_jobs) {
  reset_ai_embedding_refresh_job_list(out_jobs);
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

  std::vector<AiEmbeddingRefreshJobRecord> jobs;
  const kernel_status status =
      prepare_refresh_job_records(
          handle,
          records,
          timestamp_map(timestamp_records),
          force_refresh != 0,
          jobs);
  if (status.code != KERNEL_OK) {
    return status;
  }

  return fill_ai_embedding_refresh_job_list(jobs, out_jobs);
}

extern "C" kernel_status kernel_prepare_changed_ai_embedding_refresh_jobs(
    kernel_handle* handle,
    const char* changed_paths_lf,
    const std::size_t limit,
    kernel_ai_embedding_refresh_job_list* out_jobs) {
  reset_ai_embedding_refresh_job_list(out_jobs);
  if (handle == nullptr || out_jobs == nullptr || limit == 0) {
    return kernel::core::make_status(KERNEL_ERROR_INVALID_ARGUMENT);
  }

  kernel_path_list filtered_paths{};
  const kernel_status filter_status =
      kernel_filter_changed_markdown_paths(changed_paths_lf, &filtered_paths);
  if (filter_status.code != KERNEL_OK) {
    return filter_status;
  }
  const std::vector<std::string> rel_paths = copy_path_list(filtered_paths);
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

  std::vector<AiEmbeddingRefreshJobRecord> jobs;
  const kernel_status status =
      prepare_refresh_job_records(handle, records, timestamp_map(timestamp_records), false, jobs);
  if (status.code != KERNEL_OK) {
    return status;
  }

  return fill_ai_embedding_refresh_job_list(jobs, out_jobs);
}

extern "C" kernel_status kernel_update_ai_embedding(
    kernel_handle* handle,
    const char* note_rel_path,
    const float* values,
    const std::size_t value_count) {
  std::string normalized_rel_path;
  if (handle == nullptr || values == nullptr || value_count == 0 ||
      !normalize_note_rel_path(note_rel_path, normalized_rel_path)) {
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
      !normalize_note_rel_path(note_rel_path, normalized_rel_path)) {
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

extern "C" kernel_status kernel_query_ai_embedding_top_notes(
    kernel_handle* handle,
    const float* query_values,
    const std::size_t query_value_count,
    const char* exclude_rel_path,
    const std::size_t limit,
    kernel_search_results* out_results) {
  reset_search_results(out_results);
  std::string normalized_exclude_rel_path;
  if (handle == nullptr || query_values == nullptr || query_value_count == 0 || limit == 0 ||
      out_results == nullptr ||
      !normalize_optional_exclude_rel_path(exclude_rel_path, normalized_exclude_rel_path)) {
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

  return fill_search_results(hits, out_results);
}

extern "C" void kernel_free_ai_embedding_timestamp_list(
    kernel_ai_embedding_timestamp_list* timestamps) {
  free_ai_embedding_timestamp_list_impl(timestamps);
}

extern "C" void kernel_free_ai_embedding_refresh_job_list(
    kernel_ai_embedding_refresh_job_list* jobs) {
  free_ai_embedding_refresh_job_list_impl(jobs);
}
