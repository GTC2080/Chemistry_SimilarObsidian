// Reason: Expose AI embedding metadata and timestamp ABI separately from cache mutation/query flows.

#include "kernel/c_api.h"

#include "core/kernel_ai_embedding_cache_shared.h"
#include "core/kernel_internal.h"
#include "core/kernel_shared.h"
#include "storage/storage.h"

#include <mutex>
#include <vector>

extern "C" kernel_status kernel_upsert_ai_embedding_note_metadata(
    kernel_handle* handle,
    const kernel_ai_embedding_note_metadata* metadata) {
  kernel::storage::AiEmbeddingNoteMetadataRecord record{};
  if (handle == nullptr || !kernel::core::ai::metadata_to_record(metadata, record)) {
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
  kernel::core::ai::reset_ai_embedding_timestamp_list(out_timestamps);
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

  return kernel::core::ai::fill_ai_embedding_timestamp_list(records, out_timestamps);
}

extern "C" void kernel_free_ai_embedding_timestamp_list(
    kernel_ai_embedding_timestamp_list* timestamps) {
  kernel::core::ai::free_ai_embedding_timestamp_list_impl(timestamps);
}
