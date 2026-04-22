// Reason: This file implements deterministic local state paths for a vault without polluting the vault itself.

#include "vault/state_paths.h"

#include "platform/platform.h"
#include "vault/revision.h"

#include <string>

namespace kernel::vault {

std::filesystem::path state_dir_for_vault(const std::filesystem::path& vault_root) {
  std::filesystem::path local_app_data;
  const std::error_code ec = kernel::platform::local_app_data_directory(local_app_data);
  if (ec) {
    return {};
  }

  const std::string normalized = vault_root.lexically_normal().generic_string();
  const std::string revision = compute_content_revision(normalized);
  const std::string digest = revision.substr(std::string("v1:sha256:").size());

  return local_app_data / "ChemKernel" / "vaults" / digest;
}

std::filesystem::path recovery_journal_path(const std::filesystem::path& state_dir) {
  return state_dir / "recovery.journal";
}

std::filesystem::path storage_db_path(const std::filesystem::path& state_dir) {
  return state_dir / "state.sqlite3";
}

}  // namespace kernel::vault
