// Reason: Keep watcher-backoff reopen recovery regressions together so interrupted apply and rebuild scenarios can stay separate.

#include "kernel/c_api.h"

#include "api/kernel_api_runtime_recovery_interruptions.h"
#include "api/kernel_api_test_support.h"
#include "core/kernel_internal.h"
#include "support/test_support.h"
#include "third_party/sqlite/sqlite3.h"
#include "watcher/session.h"

#include <filesystem>
#include <string>
#include <system_error>

namespace {

void test_close_during_watcher_fault_backoff_leaves_delete_for_reopen_catch_up() {
  const auto vault = make_temp_vault();
  const auto db_path = storage_db_for_vault(vault);
  kernel_handle* handle = nullptr;
  expect_ok(kernel_open_vault(vault.string().c_str(), &handle));

  const std::string content =
      "# Backoff Delete\n"
      "backoff-delete-token\n"
      "Tags: #backoffdelete\n"
      "[[BackoffDeleteLink]]\n";
  kernel_note_metadata metadata{};
  kernel_write_disposition disposition{};
  expect_ok(kernel_write_note(
      handle,
      "backoff-delete.md",
      content.data(),
      content.size(),
      nullptr,
      &metadata,
      &disposition));
  require_index_ready(handle, "watcher backoff delete test should start from READY");

  kernel::watcher::inject_next_poll_errors(handle->watcher_session, std::errc::io_error, 20);
  require_eventually(
      [&]() {
        kernel_state_snapshot snapshot{};
        return kernel_get_state(handle, &snapshot).code == KERNEL_OK &&
               snapshot.index_state == KERNEL_INDEX_UNAVAILABLE;
      },
      "watcher should enter UNAVAILABLE backoff before closed-window delete");

  std::filesystem::remove(vault / "backoff-delete.md");
  expect_ok(kernel_close(handle));

  sqlite3* db = open_sqlite_readonly(db_path);
  require_true(
      query_single_int(db, "SELECT COUNT(*) FROM notes WHERE rel_path='backoff-delete.md' AND is_deleted=0;") == 1,
      "close during watcher backoff should leave the stale live note row for reopen catch-up to reconcile");
  require_true(
      query_single_int(
          db,
          "SELECT COUNT(*) FROM note_tags WHERE note_id=(SELECT note_id FROM notes WHERE rel_path='backoff-delete.md');") ==
          1,
      "close during watcher backoff should leave stale note tags until reopen catch-up");
  require_true(
      query_single_int(
          db,
          "SELECT COUNT(*) FROM note_links WHERE note_id=(SELECT note_id FROM notes WHERE rel_path='backoff-delete.md');") ==
          1,
      "close during watcher backoff should leave stale note links until reopen catch-up");
  sqlite3_close(db);

  handle = nullptr;
  expect_ok(kernel_open_vault(vault.string().c_str(), &handle));
  require_index_ready(handle, "reopen after watcher-backoff delete should settle to READY");

  kernel_search_results results{};
  expect_ok(kernel_search_notes(handle, "backoff-delete-token", &results));
  require_true(results.count == 0, "reopen catch-up should remove stale search hits left by a delete during watcher backoff");
  kernel_free_search_results(&results);

  db = open_sqlite_readonly(db_path);
  require_true(
      query_single_int(db, "SELECT is_deleted FROM notes WHERE rel_path='backoff-delete.md';") == 1,
      "reopen catch-up should mark the deleted note row as is_deleted=1 after watcher-backoff shutdown");
  require_true(
      query_single_int(
          db,
          "SELECT COUNT(*) FROM note_tags WHERE note_id=(SELECT note_id FROM notes WHERE rel_path='backoff-delete.md');") ==
          0,
      "reopen catch-up should clear stale tags left by a delete during watcher backoff");
  require_true(
      query_single_int(
          db,
          "SELECT COUNT(*) FROM note_links WHERE note_id=(SELECT note_id FROM notes WHERE rel_path='backoff-delete.md');") ==
          0,
      "reopen catch-up should clear stale links left by a delete during watcher backoff");
  sqlite3_close(db);

  expect_ok(kernel_close(handle));
  std::filesystem::remove_all(vault);
  std::filesystem::remove_all(state_dir_for_vault(vault));
}

void test_close_during_watcher_fault_backoff_leaves_modify_for_reopen_catch_up() {
  const auto vault = make_temp_vault();
  const auto db_path = storage_db_for_vault(vault);
  kernel_handle* handle = nullptr;
  expect_ok(kernel_open_vault(vault.string().c_str(), &handle));

  const std::string original =
      "# Backoff Modify Old\n"
      "backoff-modify-old-token\n"
      "Tags: #backoffold\n"
      "[[BackoffOldLink]]\n";
  kernel_note_metadata metadata{};
  kernel_write_disposition disposition{};
  expect_ok(kernel_write_note(
      handle,
      "backoff-modify.md",
      original.data(),
      original.size(),
      nullptr,
      &metadata,
      &disposition));
  require_index_ready(handle, "watcher backoff modify test should start from READY");

  kernel::watcher::inject_next_poll_errors(handle->watcher_session, std::errc::io_error, 20);
  require_eventually(
      [&]() {
        kernel_state_snapshot snapshot{};
        return kernel_get_state(handle, &snapshot).code == KERNEL_OK &&
               snapshot.index_state == KERNEL_INDEX_UNAVAILABLE;
      },
      "watcher should enter UNAVAILABLE backoff before closed-window modify");

  write_file_bytes(
      vault / "backoff-modify.md",
      "# Backoff Modify New\n"
      "backoff-modify-new-token\n"
      "Tags: #backoffnew\n"
      "[[BackoffNewLink]]\n");
  expect_ok(kernel_close(handle));

  sqlite3* db = open_sqlite_readonly(db_path);
  require_true(
      query_single_text(db, "SELECT title FROM notes WHERE rel_path='backoff-modify.md';") == "Backoff Modify Old",
      "close during watcher backoff should leave the stale title row for reopen catch-up to reconcile");
  require_true(
      query_single_text(
          db,
          "SELECT tag FROM note_tags WHERE note_id=(SELECT note_id FROM notes WHERE rel_path='backoff-modify.md') LIMIT 1;") ==
          "backoffold",
      "close during watcher backoff should leave stale tags until reopen catch-up");
  require_true(
      query_single_text(
          db,
          "SELECT target FROM note_links WHERE note_id=(SELECT note_id FROM notes WHERE rel_path='backoff-modify.md') LIMIT 1;") ==
          "BackoffOldLink",
      "close during watcher backoff should leave stale links until reopen catch-up");
  sqlite3_close(db);

  handle = nullptr;
  expect_ok(kernel_open_vault(vault.string().c_str(), &handle));
  require_index_ready(handle, "reopen after watcher-backoff modify should settle to READY");

  kernel_search_results results{};
  expect_ok(kernel_search_notes(handle, "backoff-modify-old-token", &results));
  require_true(results.count == 0, "reopen catch-up should remove stale search hits left by a modify during watcher backoff");
  kernel_free_search_results(&results);

  expect_ok(kernel_search_notes(handle, "backoff-modify-new-token", &results));
  require_true(results.count == 1, "reopen catch-up should index the disk-backed modified note after watcher-backoff shutdown");
  require_true(std::string(results.hits[0].rel_path) == "backoff-modify.md", "modified reopen hit should preserve rel_path");
  kernel_free_search_results(&results);

  db = open_sqlite_readonly(db_path);
  require_true(
      query_single_text(db, "SELECT title FROM notes WHERE rel_path='backoff-modify.md';") == "Backoff Modify New",
      "reopen catch-up should restore the modified disk-backed title after watcher-backoff shutdown");
  require_true(
      query_single_text(
          db,
          "SELECT tag FROM note_tags WHERE note_id=(SELECT note_id FROM notes WHERE rel_path='backoff-modify.md') LIMIT 1;") ==
          "backoffnew",
      "reopen catch-up should replace stale tags after watcher-backoff shutdown");
  require_true(
      query_single_text(
          db,
          "SELECT target FROM note_links WHERE note_id=(SELECT note_id FROM notes WHERE rel_path='backoff-modify.md') LIMIT 1;") ==
          "BackoffNewLink",
      "reopen catch-up should replace stale links after watcher-backoff shutdown");
  sqlite3_close(db);

  expect_ok(kernel_close(handle));
  std::filesystem::remove_all(vault);
  std::filesystem::remove_all(state_dir_for_vault(vault));
}

void test_close_during_watcher_fault_backoff_leaves_create_for_reopen_catch_up() {
  const auto vault = make_temp_vault();
  const auto db_path = storage_db_for_vault(vault);
  kernel_handle* handle = nullptr;
  expect_ok(kernel_open_vault(vault.string().c_str(), &handle));
  require_index_ready(handle, "watcher backoff create test should start from READY");

  kernel::watcher::inject_next_poll_errors(handle->watcher_session, std::errc::io_error, 20);
  require_eventually(
      [&]() {
        kernel_state_snapshot snapshot{};
        return kernel_get_state(handle, &snapshot).code == KERNEL_OK &&
               snapshot.index_state == KERNEL_INDEX_UNAVAILABLE;
      },
      "watcher should enter UNAVAILABLE backoff before closed-window create");

  write_file_bytes(
      vault / "backoff-create.md",
      "# Backoff Create\n"
      "backoff-create-token\n"
      "Tags: #backoffcreate\n"
      "[[BackoffCreateLink]]\n");
  expect_ok(kernel_close(handle));

  sqlite3* db = open_sqlite_readonly(db_path);
  require_true(
      query_single_int(db, "SELECT COUNT(*) FROM notes WHERE rel_path='backoff-create.md' AND is_deleted=0;") == 0,
      "close during watcher backoff should not commit a newly created note row before reopen catch-up");
  sqlite3_close(db);

  handle = nullptr;
  expect_ok(kernel_open_vault(vault.string().c_str(), &handle));
  require_index_ready(handle, "reopen after watcher-backoff create should settle to READY");

  kernel_search_results results{};
  expect_ok(kernel_search_notes(handle, "backoff-create-token", &results));
  require_true(results.count == 1, "reopen catch-up should index a note created during watcher backoff");
  require_true(std::string(results.hits[0].rel_path) == "backoff-create.md", "created reopen hit should preserve rel_path");
  kernel_free_search_results(&results);

  db = open_sqlite_readonly(db_path);
  require_true(
      query_single_text(db, "SELECT title FROM notes WHERE rel_path='backoff-create.md';") == "Backoff Create",
      "reopen catch-up should persist the created disk-backed title after watcher-backoff shutdown");
  require_true(
      query_single_text(
          db,
          "SELECT tag FROM note_tags WHERE note_id=(SELECT note_id FROM notes WHERE rel_path='backoff-create.md') LIMIT 1;") ==
          "backoffcreate",
      "reopen catch-up should persist created tags after watcher-backoff shutdown");
  require_true(
      query_single_text(
          db,
          "SELECT target FROM note_links WHERE note_id=(SELECT note_id FROM notes WHERE rel_path='backoff-create.md') LIMIT 1;") ==
          "BackoffCreateLink",
      "reopen catch-up should persist created links after watcher-backoff shutdown");
  sqlite3_close(db);

  expect_ok(kernel_close(handle));
  std::filesystem::remove_all(vault);
  std::filesystem::remove_all(state_dir_for_vault(vault));
}

}  // namespace

void run_runtime_recovery_backoff_tests() {
  test_close_during_watcher_fault_backoff_leaves_delete_for_reopen_catch_up();
  test_close_during_watcher_fault_backoff_leaves_modify_for_reopen_catch_up();
  test_close_during_watcher_fault_backoff_leaves_create_for_reopen_catch_up();
}
