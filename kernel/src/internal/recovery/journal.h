// Reason: This file owns the framed sidecar journal writes so save lifecycle details stay out of core.

#pragma once

#include "storage/storage.h"

#include <filesystem>
#include <string>
#include <system_error>

namespace kernel::recovery {

struct StartupRecoverySummary {
  std::string outcome;
  bool detected_corrupt_tail = false;
};

std::string make_operation_id();
std::error_code append_save_begin(
    const std::filesystem::path& journal_path,
    const std::string& operation_id,
    const std::string& rel_path,
    const std::filesystem::path& temp_path);
std::error_code append_save_commit(
    const std::filesystem::path& journal_path,
    const std::string& operation_id,
    const std::string& rel_path);
std::error_code count_unfinished_save_operations(
    const std::filesystem::path& journal_path,
    std::uint64_t& out_count);
std::error_code recover_startup(
    const std::filesystem::path& journal_path,
    const std::filesystem::path& vault_root,
    kernel::storage::Database& storage,
    std::uint64_t& out_pending_count,
    StartupRecoverySummary* out_summary = nullptr);

}  // namespace kernel::recovery
