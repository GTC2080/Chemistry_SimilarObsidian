// Reason: Keep closed-window and reopen catch-up disk-truth repair tests together so startup replacement scenarios can stay separate.

#include "kernel/c_api.h"

#include "api/kernel_api_runtime_recovery_disk_truth_suites.h"
#include "api/kernel_api_test_support.h"
#include "support/test_support.h"
#include "third_party/sqlite/sqlite3.h"

#include <filesystem>
#include <string>

namespace {

void test_open_vault_catches_up_external_modify_while_closed() {
  const auto vault = make_temp_vault();
  kernel_handle* handle = nullptr;
  expect_ok(kernel_open_vault(vault.string().c_str(), &handle));

  const std::string original =
      "# Catch Up Title\n"
      "catch-up-before-token\n";
  kernel_note_metadata metadata{};
  kernel_write_disposition disposition{};
  expect_ok(kernel_write_note(
      handle,
      "catch-up.md",
      original.data(),
      original.size(),
      nullptr,
      &metadata,
      &disposition));
  expect_ok(kernel_close(handle));

  write_file_bytes(
      vault / "catch-up.md",
      "# Catch Up Title\n"
      "catch-up-after-token\n");

  expect_ok(kernel_open_vault(vault.string().c_str(), &handle));
  require_eventually(
      [&]() {
        kernel_search_results old_results{};
        if (kernel_search_notes(handle, "catch-up-before-token", &old_results).code != KERNEL_OK) {
          return false;
        }
        const bool old_gone = old_results.count == 0;
        kernel_free_search_results(&old_results);

        kernel_search_results new_results{};
        if (kernel_search_notes(handle, "catch-up-after-token", &new_results).code != KERNEL_OK) {
          return false;
        }
        const bool new_present =
            new_results.count == 1 &&
            std::string(new_results.hits[0].rel_path) == "catch-up.md";
        kernel_free_search_results(&new_results);
        if (!old_gone || !new_present) {
          return false;
        }

        kernel_state_snapshot snapshot{};
        return kernel_get_state(handle, &snapshot).code == KERNEL_OK &&
               snapshot.index_state == KERNEL_INDEX_READY;
      },
      "open_vault should catch up external modifications that happened while the kernel was closed");

  expect_ok(kernel_close(handle));
  std::filesystem::remove_all(vault);
  std::filesystem::remove_all(state_dir_for_vault(vault));
}

void test_reopen_catch_up_repairs_stale_derived_state_left_by_interrupted_rebuild() {
  const auto vault = make_temp_vault();
  const auto db_path = storage_db_for_vault(vault);
  kernel_handle* handle = nullptr;
  expect_ok(kernel_open_vault(vault.string().c_str(), &handle));

  const std::string content =
      "# Reopen Repair Title\n"
      "reopen-repair-live-token\n"
      "#repairtag\n"
      "[[RepairLink]]\n";
  kernel_note_metadata metadata{};
  kernel_write_disposition disposition{};
  expect_ok(kernel_write_note(
      handle,
      "reopen-repair.md",
      content.data(),
      content.size(),
      nullptr,
      &metadata,
      &disposition));
  require_index_ready(handle, "seed write should settle before stale derived-state injection");
  expect_ok(kernel_close(handle));

  sqlite3* db = open_sqlite_readwrite(db_path);
  exec_sql(db, "UPDATE notes SET title='Stale Reopen Title' WHERE rel_path='reopen-repair.md';");
  exec_sql(db, "DELETE FROM note_tags WHERE note_id=(SELECT note_id FROM notes WHERE rel_path='reopen-repair.md');");
  exec_sql(
      db,
      "INSERT INTO note_tags(note_id, tag) VALUES((SELECT note_id FROM notes WHERE rel_path='reopen-repair.md'), 'stale_reopen_tag');");
  exec_sql(db, "DELETE FROM note_links WHERE note_id=(SELECT note_id FROM notes WHERE rel_path='reopen-repair.md');");
  exec_sql(
      db,
      "INSERT INTO note_links(note_id, target) VALUES((SELECT note_id FROM notes WHERE rel_path='reopen-repair.md'), 'StaleReopenLink');");
  exec_sql(db, "DELETE FROM note_fts WHERE rowid=(SELECT note_id FROM notes WHERE rel_path='reopen-repair.md');");
  exec_sql(
      db,
      "INSERT INTO note_fts(rowid, title, body) VALUES("
      " (SELECT note_id FROM notes WHERE rel_path='reopen-repair.md'),"
      " 'Stale Reopen Title',"
      " 'reopen-repair-stale-token');");
  sqlite3_close(db);

  expect_ok(kernel_open_vault(vault.string().c_str(), &handle));

  require_eventually(
      [&]() {
        kernel_state_snapshot snapshot{};
        if (kernel_get_state(handle, &snapshot).code != KERNEL_OK ||
            snapshot.index_state != KERNEL_INDEX_READY) {
          return false;
        }

        kernel_search_results stale_results{};
        if (kernel_search_notes(handle, "reopen-repair-stale-token", &stale_results).code != KERNEL_OK) {
          return false;
        }
        const bool stale_gone = stale_results.count == 0;
        kernel_free_search_results(&stale_results);

        kernel_search_results live_results{};
        if (kernel_search_notes(handle, "reopen-repair-live-token", &live_results).code != KERNEL_OK) {
          return false;
        }
        const bool live_present =
            live_results.count == 1 &&
            std::string(live_results.hits[0].rel_path) == "reopen-repair.md";
        kernel_free_search_results(&live_results);
        return stale_gone && live_present;
      },
      "startup catch-up should repair stale derived state left behind while the kernel was closed");

  db = open_sqlite_readonly(db_path);
  require_true(
      query_single_text(db, "SELECT title FROM notes WHERE rel_path='reopen-repair.md';") == "Reopen Repair Title",
      "reopen catch-up should restore the disk-backed title");
  require_true(
      query_single_text(
          db,
          "SELECT tag FROM note_tags WHERE note_id=(SELECT note_id FROM notes WHERE rel_path='reopen-repair.md') LIMIT 1;") ==
          "repairtag",
      "reopen catch-up should replace stale tags with the disk-backed tag set");
  require_true(
      query_single_text(
          db,
          "SELECT target FROM note_links WHERE note_id=(SELECT note_id FROM notes WHERE rel_path='reopen-repair.md') LIMIT 1;") ==
          "RepairLink",
      "reopen catch-up should replace stale links with the disk-backed link set");
  sqlite3_close(db);

  expect_ok(kernel_close(handle));
  std::filesystem::remove_all(vault);
  std::filesystem::remove_all(state_dir_for_vault(vault));
}

}  // namespace

void run_runtime_recovery_disk_truth_catchup_tests() {
  test_open_vault_catches_up_external_modify_while_closed();
  test_reopen_catch_up_repairs_stale_derived_state_left_by_interrupted_rebuild();
}
