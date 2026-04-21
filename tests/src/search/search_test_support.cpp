// Reason: Share the internal search test database bootstrap across slimmer search suites.

#include "search/search_test_support.h"

#include "support/test_support.h"

namespace search_tests {

kernel::storage::Database open_search_db(const std::filesystem::path& vault) {
  kernel::storage::Database db;
  const std::error_code ec = kernel::storage::open_or_create(storage_db_for_vault(vault), db);
  require_true(!ec, "search db should open");
  return db;
}

}  // namespace search_tests
