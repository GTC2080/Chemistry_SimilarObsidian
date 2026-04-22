// Reason: This file isolates watcher close and catch-up edge coverage so steady-state smoke tests stay compact.

#include "kernel/c_api.h"

#include "api/kernel_api_test_support.h"
#include "api/kernel_api_watcher_smoke_suites.h"
#include "index/refresh.h"
#include "support/test_support.h"
#include "third_party/sqlite/sqlite3.h"

#include <chrono>
#include <filesystem>
#include <string>
#include <system_error>
#include <thread>

namespace {

void test_close_stops_background_watcher_until_reopen() {
  const auto vault = make_temp_vault();
  kernel_handle* handle = nullptr;
  expect_ok(kernel_open_vault(vault.string().c_str(), &handle));
  require_index_ready(handle, "close lifecycle test should start from a ready state");

  expect_ok(kernel_close(handle));

  const std::string rel_path = "closed-window.md";
  const std::string token = "closed-window-token";
  write_file_bytes(vault / rel_path, "# Closed Window\n" + token + "\n");
  std::this_thread::sleep_for(std::chrono::milliseconds(300));

  sqlite3* readonly_db = open_sqlite_readonly(storage_db_for_vault(vault));
  require_true(
      query_single_int(readonly_db, "SELECT COUNT(*) FROM notes WHERE is_deleted=0;") == 0,
      "closed kernel should not keep indexing external changes after kernel_close");
  sqlite3_close(readonly_db);

  handle = nullptr;
  expect_ok(kernel_open_vault(vault.string().c_str(), &handle));
  require_index_ready(handle, "reopen should catch up external changes that happened while closed");

  kernel_search_results results{};
  expect_ok(kernel_search_notes(handle, token.c_str(), &results));
  require_true(results.count == 1, "reopen should make closed-window external create searchable");
  require_true(
      std::string(results.hits[0].rel_path) == rel_path,
      "reopen should preserve rel_path after closed-window catch-up");
  kernel_free_search_results(&results);

  expect_ok(kernel_close(handle));
  std::filesystem::remove_all(vault);
  std::filesystem::remove_all(state_dir_for_vault(vault));
}

void test_close_releases_watcher_handles_so_vault_can_be_renamed() {
  const auto vault = make_temp_vault();
  const auto renamed_vault = vault.parent_path() / (vault.filename().string() + "-renamed");

  kernel_handle* handle = nullptr;
  expect_ok(kernel_open_vault(vault.string().c_str(), &handle));
  require_index_ready(handle, "close handle-release test should start from a ready state");

  expect_ok(kernel_close(handle));

  std::error_code rename_ec;
  std::filesystem::rename(vault, renamed_vault, rename_ec);
  require_true(!rename_ec, "kernel_close should release watcher handles so the vault directory can be renamed");

  std::filesystem::rename(renamed_vault, vault, rename_ec);
  require_true(!rename_ec, "renamed vault should be movable back after close");

  std::filesystem::remove_all(vault);
  std::filesystem::remove_all(state_dir_for_vault(vault));
}

void test_close_during_delayed_catch_up_does_not_commit_catch_up_results() {
  const auto vault = make_temp_vault();
  const auto db_path = storage_db_for_vault(vault);
  kernel::index::inject_full_rescan_delay_ms(500, 1);

  kernel_handle* handle = nullptr;
  expect_ok(kernel_open_vault(vault.string().c_str(), &handle));

  require_eventually(
      [&]() {
        kernel_state_snapshot snapshot{};
        return kernel_get_state(handle, &snapshot).code == KERNEL_OK &&
               snapshot.index_state == KERNEL_INDEX_CATCHING_UP;
      },
      "delayed catch-up should be observable before close");

  write_file_bytes(
      vault / "close-during-catchup.md",
      "# Close During Catch Up\nclose-during-catchup-token\n");

  expect_ok(kernel_close(handle));
  kernel::index::inject_full_rescan_delay_ms(0, 0);

  sqlite3* db = open_sqlite_readonly(db_path);
  require_true(
      query_single_int(db, "SELECT COUNT(*) FROM notes WHERE rel_path='close-during-catchup.md' AND is_deleted=0;") == 0,
      "closing during delayed catch-up should not commit catch-up results into sqlite");
  sqlite3_close(db);

  expect_ok(kernel_open_vault(vault.string().c_str(), &handle));
  require_eventually(
      [&]() {
        kernel_search_results results{};
        if (kernel_search_notes(handle, "close-during-catchup-token", &results).code != KERNEL_OK) {
          return false;
        }
        const bool indexed =
            results.count == 1 &&
            std::string(results.hits[0].rel_path) == "close-during-catchup.md";
        kernel_free_search_results(&results);
        if (!indexed) {
          return false;
        }

        kernel_state_snapshot snapshot{};
        return kernel_get_state(handle, &snapshot).code == KERNEL_OK &&
               snapshot.index_state == KERNEL_INDEX_READY &&
               snapshot.indexed_note_count == 1;
      },
      "reopen should reconcile the file through a fresh catch-up");

  expect_ok(kernel_close(handle));
  std::filesystem::remove_all(vault);
  std::filesystem::remove_all(state_dir_for_vault(vault));
}

void test_initial_catch_up_and_watcher_poll_do_not_double_apply_external_create() {
  const auto vault = make_temp_vault();
  const auto db_path = storage_db_for_vault(vault);

  kernel::index::inject_full_rescan_delay_ms(300, 1000);

  kernel_handle* handle = nullptr;
  expect_ok(kernel_open_vault(vault.string().c_str(), &handle));

  require_eventually(
      [&]() {
        kernel_state_snapshot snapshot{};
        return kernel_get_state(handle, &snapshot).code == KERNEL_OK &&
               snapshot.index_state == KERNEL_INDEX_CATCHING_UP;
      },
      "delayed initial catch-up should become observable before creating the external note");

  write_file_bytes(
      vault / "catchup-created.md",
      "# Catchup Create\ncatchup-create-token\n");

  require_index_ready(handle, "delayed initial catch-up should eventually settle back to READY");
  kernel::index::inject_full_rescan_delay_ms(0, 0);

  kernel_search_results results{};
  expect_ok(kernel_search_notes(handle, "catchup-create-token", &results));
  require_true(results.count == 1, "create during initial catch-up should become searchable exactly once");
  require_true(
      std::string(results.hits[0].rel_path) == "catchup-created.md",
      "catch-up create hit should preserve rel_path");
  kernel_free_search_results(&results);

  kernel_state_snapshot snapshot{};
  expect_ok(kernel_get_state(handle, &snapshot));
  require_true(snapshot.indexed_note_count == 1, "catch-up plus later watcher poll should not double-count the created note");

  sqlite3* db = open_sqlite_readonly(db_path);
  require_true(
      query_single_int(db, "SELECT COUNT(*) FROM notes WHERE is_deleted=0 AND rel_path='catchup-created.md';") == 1,
      "catch-up plus later watcher poll should leave exactly one live notes row");
  sqlite3_close(db);

  expect_ok(kernel_close(handle));
  std::filesystem::remove_all(vault);
  std::filesystem::remove_all(state_dir_for_vault(vault));
}

}  // namespace

void run_kernel_api_watcher_close_edge_tests() {
  test_close_stops_background_watcher_until_reopen();
  test_close_releases_watcher_handles_so_vault_can_be_renamed();
  test_close_during_delayed_catch_up_does_not_commit_catch_up_results();
  test_initial_catch_up_and_watcher_poll_do_not_double_apply_external_create();
}
