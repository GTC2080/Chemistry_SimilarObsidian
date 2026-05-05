// Reason: Share AI embedding refresh-job selection rules across full and changed-path flows.

#pragma once

#include "core/kernel_ai_embedding_cache_shared.h"
#include "storage/storage.h"

#include <cstdint>
#include <string>
#include <unordered_map>
#include <vector>

struct kernel_handle;

namespace kernel::core::ai {

std::unordered_map<std::string, std::int64_t> ai_embedding_timestamp_map(
    const std::vector<kernel::storage::AiEmbeddingTimestampRecord>& records);

kernel_status prepare_ai_embedding_refresh_job_records(
    kernel_handle* handle,
    const std::vector<kernel::storage::NoteCatalogRecord>& records,
    const std::unordered_map<std::string, std::int64_t>& timestamps,
    bool force_refresh,
    std::vector<AiEmbeddingRefreshJobRecord>& out_jobs);

}  // namespace kernel::core::ai
