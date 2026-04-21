// Reason: This file centralizes state_dir and recovery.journal path derivation away from core orchestration.

#pragma once

#include <filesystem>

namespace kernel::vault {

std::filesystem::path state_dir_for_vault(const std::filesystem::path& vault_root);
std::filesystem::path storage_db_path(const std::filesystem::path& state_dir);
std::filesystem::path recovery_journal_path(const std::filesystem::path& state_dir);

}  // namespace kernel::vault
