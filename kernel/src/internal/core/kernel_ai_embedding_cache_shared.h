// Reason: Share AI embedding cache ABI marshaling and path helpers across split API files.

#pragma once

#include "kernel/c_api.h"
#include "storage/storage.h"

#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

struct kernel_handle;

namespace kernel::core::ai {

struct AiEmbeddingRefreshJobRecord {
  std::string rel_path;
  std::string title;
  std::string absolute_path;
  std::int64_t created_at = 0;
  std::int64_t updated_at = 0;
  std::string content;
};

void free_ai_embedding_timestamp_list_impl(kernel_ai_embedding_timestamp_list* timestamps);
void reset_ai_embedding_timestamp_list(kernel_ai_embedding_timestamp_list* timestamps);
void reset_search_results(kernel_search_results* out_results);
void free_ai_embedding_refresh_job_list_impl(kernel_ai_embedding_refresh_job_list* jobs);
void reset_ai_embedding_refresh_job_list(kernel_ai_embedding_refresh_job_list* jobs);

std::error_code read_note_content(
    kernel_handle* handle,
    std::string_view rel_path,
    std::string& out_content);
bool normalize_note_rel_path(const char* rel_path, std::string& out_rel_path);
bool normalize_optional_exclude_rel_path(const char* rel_path, std::string& out_rel_path);
bool metadata_to_record(
    const kernel_ai_embedding_note_metadata* metadata,
    kernel::storage::AiEmbeddingNoteMetadataRecord& out_record);

kernel_status fill_ai_embedding_timestamp_list(
    const std::vector<kernel::storage::AiEmbeddingTimestampRecord>& records,
    kernel_ai_embedding_timestamp_list* out_timestamps);
kernel_status fill_search_results(
    const std::vector<kernel::storage::NoteListHit>& hits,
    kernel_search_results* out_results);
kernel_status fill_ai_embedding_refresh_job_list(
    const std::vector<AiEmbeddingRefreshJobRecord>& records,
    kernel_ai_embedding_refresh_job_list* out_jobs);

std::vector<std::string> copy_path_list(const kernel_path_list& paths);

}  // namespace kernel::core::ai
