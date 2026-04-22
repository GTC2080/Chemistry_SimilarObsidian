// Reason: Keep interrupted rebuild and watcher-apply recovery regressions together so watcher-backoff scenarios can stay smaller.

#include "kernel/c_api.h"

#include "api/kernel_api_runtime_recovery_interruptions.h"
#include "api/kernel_api_test_support.h"
#include "core/kernel_internal.h"
#include "index/refresh.h"
#include "storage/storage.h"
#include "support/test_support.h"
#include "third_party/sqlite/sqlite3.h"
#include "watcher/integration.h"

#include <filesystem>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

namespace {

void test_reopen_catch_up_repairs_partial_state_left_by_interrupted_background_rebuild() {
  const auto vault = make_temp_vault();
  const auto db_path = storage_db_for_vault(vault);

  kernel_handle* handle = nullptr;
  expect_ok(kernel_open_vault(vault.string().c_str(), &handle));
  require_index_ready(handle, "interrupted background rebuild recovery test should start from READY");

  const std::string live_content =
      "# Interrupted Rebuild Live\n"
      "interrupted-rebuild-live-token\n"
      "#interruptedlive\n"
      "[[InterruptedLiveLink]]\n";
  kernel_note_metadata metadata{};
  kernel_write_disposition disposition{};
  expect_ok(kernel_write_note(
      handle,
      "interrupted-rebuild-live.md",
      live_content.data(),
      live_content.size(),
      nullptr,
      &metadata,
      &disposition));
  require_index_ready(handle, "seed write should settle before injecting stale ghost rows");

  {
    std::lock_guard lock(handle->storage_mutex);
    exec_sql(
        handle->storage.connection,
        "INSERT INTO notes(rel_path, title, file_size, mtime_ns, content_revision, is_deleted) "
        "VALUES('interrupted-rebuild-ghost.md', 'Interrupted Ghost Title', 1, 1, 'ghost-revision', 0);");
    exec_sql(
        handle->storage.connection,
        "INSERT INTO note_tags(note_id, tag) "
        "VALUES((SELECT note_id FROM notes WHERE rel_path='interrupted-rebuild-ghost.md'), 'ghosttag');");
    exec_sql(
        handle->storage.connection,
        "INSERT INTO note_links(note_id, target) "
        "VALUES((SELECT note_id FROM notes WHERE rel_path='interrupted-rebuild-ghost.md'), 'InterruptedGhostLink');");
    exec_sql(
        handle->storage.connection,
        "INSERT INTO note_fts(rowid, title, body) VALUES("
        " (SELECT note_id FROM notes WHERE rel_path='interrupted-rebuild-ghost.md'),"
        " 'Interrupted Ghost Title',"
        " 'interrupted-rebuild-ghost-token');");
  }

  kernel_search_results results{};
  expect_ok(kernel_search_notes(handle, "interrupted-rebuild-ghost-token", &results));
  require_true(results.count == 1, "stale ghost note should be searchable before interrupted rebuild");
  kernel_free_search_results(&results);

  kernel::index::inject_full_rescan_interrupt_after_refresh_phase(1);
  expect_ok(kernel_start_rebuild_index(handle));

  const kernel_status join_status = kernel_join_rebuild_index(handle);
  require_true(
      join_status.code == KERNEL_ERROR_IO,
      "interrupted background rebuild should currently surface as an IO-class failure");

  kernel_state_snapshot snapshot{};
  expect_ok(kernel_get_state(handle, &snapshot));
  require_true(
      snapshot.index_state == KERNEL_INDEX_UNAVAILABLE,
      "interrupted background rebuild should degrade runtime state before reopen recovery");

  expect_ok(kernel_search_notes(handle, "interrupted-rebuild-ghost-token", &results));
  require_true(
      results.count == 1,
      "interrupting after refresh phase should leave stale ghost rows behind before reopen catch-up");
  kernel_free_search_results(&results);

  expect_ok(kernel_close(handle));

  expect_ok(kernel_open_vault(vault.string().c_str(), &handle));
  require_eventually(
      [&]() {
        kernel_search_results ghost_results{};
        if (kernel_search_notes(handle, "interrupted-rebuild-ghost-token", &ghost_results).code != KERNEL_OK) {
          return false;
        }
        const bool ghost_gone = ghost_results.count == 0;
        kernel_free_search_results(&ghost_results);

        kernel_search_results live_results{};
        if (kernel_search_notes(handle, "interrupted-rebuild-live-token", &live_results).code != KERNEL_OK) {
          return false;
        }
        const bool live_present =
            live_results.count == 1 &&
            std::string(live_results.hits[0].rel_path) == "interrupted-rebuild-live.md";
        kernel_free_search_results(&live_results);

        kernel_state_snapshot reopened{};
        return ghost_gone && live_present &&
               kernel_get_state(handle, &reopened).code == KERNEL_OK &&
               reopened.index_state == KERNEL_INDEX_READY &&
               reopened.indexed_note_count == 1;
      },
      "reopen catch-up should repair stale ghost rows left by interrupted background rebuild");

  sqlite3* db = open_sqlite_readonly(db_path);
  require_true(
      query_single_int(
          db,
          "SELECT COUNT(*) FROM notes WHERE rel_path='interrupted-rebuild-ghost.md' AND is_deleted=0;") == 0,
      "reopen catch-up should retire the stale ghost note row left by interrupted rebuild");
  require_true(
      query_single_int(
          db,
          "SELECT COUNT(*) FROM notes WHERE rel_path='interrupted-rebuild-live.md' AND is_deleted=0;") == 1,
      "reopen catch-up should preserve the live on-disk note after interrupted rebuild");
  sqlite3_close(db);

  expect_ok(kernel_close(handle));
  std::filesystem::remove_all(vault);
  std::filesystem::remove_all(state_dir_for_vault(vault));
}

void test_reopen_catch_up_repairs_partial_state_left_by_interrupted_watcher_apply() {
  const auto vault = make_temp_vault();
  const auto db_path = storage_db_for_vault(vault);

  write_file_bytes(
      vault / "watcher-apply-one.md",
      "# Watcher Apply One\nwatcher-apply-one-token\n");
  write_file_bytes(
      vault / "watcher-apply-two.md",
      "# Watcher Apply Two\nwatcher-apply-two-token\n");

  kernel_handle* handle = nullptr;
  expect_ok(kernel_open_vault(vault.string().c_str(), &handle));
  expect_ok(kernel_close(handle));

  auto db = kernel::storage::Database{};
  require_true(
      !kernel::storage::open_or_create(storage_db_for_vault(vault), db),
      "interrupted watcher apply test should open storage db");
  require_true(
      !kernel::storage::ensure_schema_v1(db),
      "interrupted watcher apply test should ensure schema");

  const std::vector<kernel::watcher::CoalescedAction> actions = {
      {kernel::watcher::CoalescedActionKind::RefreshPath, "watcher-apply-one.md", ""},
      {kernel::watcher::CoalescedActionKind::RefreshPath, "watcher-apply-two.md", ""}};

  kernel::watcher::inject_apply_actions_delay_after_count(1, 500, 1);

  std::error_code apply_ec;
  std::jthread apply_thread([&](std::stop_token stop_token) {
    apply_ec = kernel::watcher::apply_actions(db, vault, actions, stop_token);
  });

  require_eventually(
      [&]() {
        sqlite3* readonly_db = open_sqlite_readonly(db_path);
        const bool first_present =
            query_single_int(
                readonly_db,
                "SELECT COUNT(*) FROM notes WHERE rel_path='watcher-apply-one.md' AND is_deleted=0;") == 1;
        const bool second_absent =
            query_single_int(
                readonly_db,
                "SELECT COUNT(*) FROM notes WHERE rel_path='watcher-apply-two.md' AND is_deleted=0;") == 0;
        sqlite3_close(readonly_db);
        return first_present && second_absent;
      },
      "interrupted watcher apply test should observe partial state after the first action commits");

  apply_thread.request_stop();
  apply_thread.join();

  require_true(
      apply_ec == std::make_error_code(std::errc::operation_canceled),
      "interrupted watcher apply should surface operation_canceled once stop is requested mid-apply");

  sqlite3* readonly_db = open_sqlite_readonly(db_path);
  require_true(
      query_single_int(
          readonly_db,
          "SELECT COUNT(*) FROM notes WHERE rel_path='watcher-apply-one.md' AND is_deleted=0;") == 1,
      "interrupted watcher apply should keep the first committed action");
  require_true(
      query_single_int(
          readonly_db,
          "SELECT COUNT(*) FROM notes WHERE rel_path='watcher-apply-two.md' AND is_deleted=0;") == 0,
      "interrupted watcher apply should leave later actions unapplied before reopen catch-up");
  sqlite3_close(readonly_db);
  kernel::storage::close(db);

  expect_ok(kernel_open_vault(vault.string().c_str(), &handle));
  require_eventually(
      [&]() {
        kernel_search_results first_results{};
        if (kernel_search_notes(handle, "watcher-apply-one-token", &first_results).code != KERNEL_OK) {
          return false;
        }
        const bool first_present =
            first_results.count == 1 &&
            std::string(first_results.hits[0].rel_path) == "watcher-apply-one.md";
        kernel_free_search_results(&first_results);

        kernel_search_results second_results{};
        if (kernel_search_notes(handle, "watcher-apply-two-token", &second_results).code != KERNEL_OK) {
          return false;
        }
        const bool second_present =
            second_results.count == 1 &&
            std::string(second_results.hits[0].rel_path) == "watcher-apply-two.md";
        kernel_free_search_results(&second_results);

        kernel_state_snapshot snapshot{};
        return first_present && second_present &&
               kernel_get_state(handle, &snapshot).code == KERNEL_OK &&
               snapshot.index_state == KERNEL_INDEX_READY &&
               snapshot.indexed_note_count == 2;
      },
      "reopen catch-up should repair the remaining watcher apply work after interrupted mid-apply shutdown");

  expect_ok(kernel_close(handle));
  std::filesystem::remove_all(vault);
  std::filesystem::remove_all(state_dir_for_vault(vault));
}

}  // namespace

void run_runtime_recovery_interrupted_apply_tests() {
  test_reopen_catch_up_repairs_partial_state_left_by_interrupted_background_rebuild();
  test_reopen_catch_up_repairs_partial_state_left_by_interrupted_watcher_apply();
}
