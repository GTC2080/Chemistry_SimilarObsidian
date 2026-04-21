// Reason: Share watcher test temp-root and database bootstrap helpers across slimmer watcher suites.

#include "watcher/watcher_test_support.h"

#include "support/test_support.h"

namespace watcher_tests {

std::filesystem::path make_temp_watch_root() {
  return make_temp_vault("chem_kernel_watcher_test_");
}

kernel::storage::Database open_search_db(const std::filesystem::path& vault) {
  kernel::storage::Database db;
  const std::error_code ec = kernel::storage::open_or_create(storage_db_for_vault(vault), db);
  require_true(!ec, "watcher test search db should open");
  return db;
}

}  // namespace watcher_tests
