// Reason: This file implements deterministic Nexus-managed state paths inside a vault.

#include "vault/state_paths.h"

namespace kernel::vault {

std::filesystem::path state_dir_for_vault(const std::filesystem::path& vault_root) {
  return vault_root.lexically_normal() / ".nexus" / "kernel";
}

std::filesystem::path recovery_journal_path(const std::filesystem::path& state_dir) {
  return state_dir / "recovery.journal";
}

std::filesystem::path storage_db_path(const std::filesystem::path& state_dir) {
  return state_dir / "state.sqlite3";
}

}  // namespace kernel::vault
