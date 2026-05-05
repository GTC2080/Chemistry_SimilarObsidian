// Reason: Own AI embedding cache DTO marshaling and path helpers away from public ABI flows.

#include "core/kernel_ai_embedding_cache_shared.h"

#include "core/kernel_internal.h"
#include "core/kernel_shared.h"
#include "platform/platform.h"

#include <cstring>
#include <new>
#include <string>
#include <string_view>
#include <vector>

namespace {

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

}  // namespace

namespace kernel::core::ai {

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

}  // namespace kernel::core::ai
