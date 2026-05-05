// Reason: Own AI embedding refresh-job selection rules separate from public ABI entrypoints.

#include "core/kernel_ai_embedding_refresh_jobs_shared.h"

#include "core/kernel_internal.h"
#include "core/kernel_shared.h"
#include "storage/storage.h"

#include <filesystem>
#include <mutex>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace {

std::int64_t note_updated_at_seconds(const kernel::storage::NoteCatalogRecord& record) {
  return static_cast<std::int64_t>(record.mtime_ns / 1000000000ULL);
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

std::filesystem::path absolute_note_path(kernel_handle* handle, std::string_view rel_path) {
  return (handle->paths.root / std::filesystem::path(std::string(rel_path))).lexically_normal();
}

}  // namespace

namespace kernel::core::ai {

std::unordered_map<std::string, std::int64_t> ai_embedding_timestamp_map(
    const std::vector<kernel::storage::AiEmbeddingTimestampRecord>& records) {
  std::unordered_map<std::string, std::int64_t> timestamps;
  timestamps.reserve(records.size());
  for (const auto& record : records) {
    timestamps.emplace(record.rel_path, record.updated_at);
  }
  return timestamps;
}

kernel_status prepare_ai_embedding_refresh_job_records(
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

}  // namespace kernel::core::ai
