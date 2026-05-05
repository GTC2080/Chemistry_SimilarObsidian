// Reason: This file exposes the kernel-owned AI embedding cache ABI to host shells.

#include "kernel/c_api.h"

#include "core/kernel_internal.h"
#include "core/kernel_shared.h"
#include "storage/storage.h"

#include <mutex>
#include <new>
#include <string>
#include <string_view>
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
