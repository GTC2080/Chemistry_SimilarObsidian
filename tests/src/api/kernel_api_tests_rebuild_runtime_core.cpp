// Reason: Keep synchronous rebuild visibility and disk-truth repair coverage separate from background-task semantics.

#include "kernel/c_api.h"

#include "api/kernel_api_rebuild_runtime_suites.h"
#include "api/kernel_api_test_support.h"
#include "core/kernel_internal.h"
#include "index/refresh.h"
#include "support/test_support.h"
#include "third_party/sqlite/sqlite3.h"

#include <filesystem>
#include <mutex>
#include <string>
#include <thread>

namespace {

void test_rebuild_index_reconciles_disk_truth_after_db_drift() {
  const auto vault = make_temp_vault();
  const auto db_path = storage_db_for_vault(vault);
  kernel_handle* handle = nullptr;
  expect_ok(kernel_open_vault(vault.string().c_str(), &handle));
  require_index_ready(handle, "rebuild test should start from a settled index state");

  const std::string content =
      "# Rebuild Title\n"
      "rebuild-live-token\n"
      "#rebuildtag\n"
      "[[RebuildLink]]\n";
  kernel_note_metadata metadata{};
  kernel_write_disposition disposition{};
  expect_ok(kernel_write_note(
      handle,
      "rebuild.md",
      content.data(),
      content.size(),
      nullptr,
      &metadata,
      &disposition));

  {
    std::lock_guard lock(handle->storage_mutex);
    exec_sql(handle->storage.connection, "UPDATE notes SET title='Stale Title' WHERE rel_path='rebuild.md';");
    exec_sql(handle->storage.connection, "DELETE FROM note_tags WHERE note_id=(SELECT note_id FROM notes WHERE rel_path='rebuild.md');");
    exec_sql(handle->storage.connection, "INSERT INTO note_tags(note_id, tag) VALUES((SELECT note_id FROM notes WHERE rel_path='rebuild.md'), 'staletag');");
    exec_sql(handle->storage.connection, "DELETE FROM note_links WHERE note_id=(SELECT note_id FROM notes WHERE rel_path='rebuild.md');");
    exec_sql(handle->storage.connection, "INSERT INTO note_links(note_id, target) VALUES((SELECT note_id FROM notes WHERE rel_path='rebuild.md'), 'StaleLink');");
    exec_sql(handle->storage.connection, "DELETE FROM note_fts WHERE rowid=(SELECT note_id FROM notes WHERE rel_path='rebuild.md');");
    exec_sql(
        handle->storage.connection,
        "INSERT INTO note_fts(rowid, title, body) VALUES("
        " (SELECT note_id FROM notes WHERE rel_path='rebuild.md'),"
        " 'Stale Title',"
        " 'stale-search-token');");
  }

  kernel_search_results results{};
  expect_ok(kernel_search_notes(handle, "stale-search-token", &results));
  require_true(results.count == 1, "stale FTS row should be visible before rebuild");
  kernel_free_search_results(&results);

  expect_ok(kernel_rebuild_index(handle));

  expect_ok(kernel_search_notes(handle, "stale-search-token", &results));
  require_true(results.count == 0, "rebuild should remove stale FTS rows");
  kernel_free_search_results(&results);

  expect_ok(kernel_search_notes(handle, "rebuild-live-token", &results));
  require_true(results.count == 1, "rebuild should restore live disk-backed FTS rows");
  require_true(std::string(results.hits[0].rel_path) == "rebuild.md", "rebuild hit should preserve rel_path");
  kernel_free_search_results(&results);

  kernel_state_snapshot snapshot{};
  expect_ok(kernel_get_state(handle, &snapshot));
  require_true(snapshot.index_state == KERNEL_INDEX_READY, "rebuild should leave index_state READY");

  sqlite3* readonly_db = open_sqlite_readonly(db_path);
  require_true(
      query_single_text(readonly_db, "SELECT title FROM notes WHERE rel_path='rebuild.md';") == "Rebuild Title",
      "rebuild should restore on-disk title");
  require_true(
      query_single_text(
          readonly_db,
          "SELECT tag FROM note_tags WHERE note_id=(SELECT note_id FROM notes WHERE rel_path='rebuild.md') LIMIT 1;") == "rebuildtag",
      "rebuild should restore on-disk tags");
  require_true(
      query_single_text(
          readonly_db,
          "SELECT target FROM note_links WHERE note_id=(SELECT note_id FROM notes WHERE rel_path='rebuild.md') LIMIT 1;") == "RebuildLink",
      "rebuild should restore on-disk links");
  sqlite3_close(readonly_db);

  expect_ok(kernel_close(handle));
  std::filesystem::remove_all(vault);
  std::filesystem::remove_all(state_dir_for_vault(vault));
}

void test_rebuild_reports_rebuilding_during_delayed_rescan() {
  const auto vault = make_temp_vault();
  kernel_handle* handle = nullptr;
  expect_ok(kernel_open_vault(vault.string().c_str(), &handle));
  require_index_ready(handle, "rebuild visibility test should start from a ready state");

  kernel::index::inject_full_rescan_delay_ms(300, 1000);

  kernel_status rebuild_status{KERNEL_ERROR_INTERNAL};
  std::jthread rebuild_thread([&]() {
    rebuild_status = kernel_rebuild_index(handle);
  });

  require_eventually(
      [&]() {
        kernel_state_snapshot snapshot{};
        return kernel_get_state(handle, &snapshot).code == KERNEL_OK &&
               snapshot.session_state == KERNEL_SESSION_OPEN &&
               snapshot.index_state == KERNEL_INDEX_REBUILDING;
      },
      "host should be able to observe REBUILDING during delayed rebuild");

  rebuild_thread.join();
  kernel::index::inject_full_rescan_delay_ms(0, 0);
  expect_ok(rebuild_status);

  kernel_state_snapshot snapshot{};
  expect_ok(kernel_get_state(handle, &snapshot));
  require_true(snapshot.session_state == KERNEL_SESSION_OPEN, "rebuild should keep session OPEN");
  require_true(snapshot.index_state == KERNEL_INDEX_READY, "rebuild should settle back to READY");

  expect_ok(kernel_close(handle));
  std::filesystem::remove_all(vault);
  std::filesystem::remove_all(state_dir_for_vault(vault));
}

}  // namespace

void run_rebuild_runtime_core_tests() {
  test_rebuild_index_reconciles_disk_truth_after_db_drift();
  test_rebuild_reports_rebuilding_during_delayed_rescan();
}
