// Reason: This file isolates rebuild-runtime API coverage so the main ABI suite can stay focused on open/write/watcher smoke behavior.

#include "kernel/c_api.h"

#include "api/kernel_api_test_support.h"
#include "core/kernel_internal.h"
#include "index/refresh.h"
#include "support/test_support.h"
#include "third_party/sqlite/sqlite3.h"

#include <filesystem>
#include <mutex>
#include <string>
#include <system_error>
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

  sqlite3* readonly_db = open_sqlite_readonly(storage_db_for_vault(vault));
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

void test_background_rebuild_start_reports_rebuilding_and_join_restores_ready() {
  const auto vault = make_temp_vault();
  kernel_handle* handle = nullptr;
  expect_ok(kernel_open_vault(vault.string().c_str(), &handle));
  require_index_ready(handle, "background rebuild start test should start from a ready state");

  kernel::index::inject_full_rescan_delay_ms(300, 1000);

  expect_ok(kernel_start_rebuild_index(handle));

  require_eventually(
      [&]() {
        kernel_state_snapshot snapshot{};
        return kernel_get_state(handle, &snapshot).code == KERNEL_OK &&
               snapshot.index_state == KERNEL_INDEX_REBUILDING;
      },
      "background rebuild should expose REBUILDING while in flight");

  expect_ok(kernel_join_rebuild_index(handle));
  kernel::index::inject_full_rescan_delay_ms(0, 0);

  kernel_state_snapshot snapshot{};
  expect_ok(kernel_get_state(handle, &snapshot));
  require_true(snapshot.index_state == KERNEL_INDEX_READY, "joined background rebuild should restore READY");

  expect_ok(kernel_close(handle));
  std::filesystem::remove_all(vault);
  std::filesystem::remove_all(state_dir_for_vault(vault));
}

void test_background_rebuild_rejects_duplicate_start_requests() {
  const auto vault = make_temp_vault();
  kernel_handle* handle = nullptr;
  expect_ok(kernel_open_vault(vault.string().c_str(), &handle));
  require_index_ready(handle, "background rebuild duplicate-start test should start from a ready state");

  kernel::index::inject_full_rescan_delay_ms(300, 1000);

  expect_ok(kernel_start_rebuild_index(handle));
  const kernel_status duplicate_start = kernel_start_rebuild_index(handle);
  require_true(
      duplicate_start.code == KERNEL_ERROR_CONFLICT,
      "duplicate background rebuild start should be rejected with KERNEL_ERROR_CONFLICT");

  expect_ok(kernel_join_rebuild_index(handle));
  kernel::index::inject_full_rescan_delay_ms(0, 0);

  expect_ok(kernel_close(handle));
  std::filesystem::remove_all(vault);
  std::filesystem::remove_all(state_dir_for_vault(vault));
}

void test_background_rebuild_failure_surfaces_through_join_and_diagnostics() {
  const auto vault = make_temp_vault();
  const auto export_path = vault / "diagnostics-background-rebuild-fault.json";
  kernel_handle* handle = nullptr;
  expect_ok(kernel_open_vault(vault.string().c_str(), &handle));
  require_index_ready(handle, "background rebuild failure test should start from a ready state");

  kernel::index::inject_full_rescan_failures(std::errc::io_error, 1);

  expect_ok(kernel_start_rebuild_index(handle));
  const kernel_status join_status = kernel_join_rebuild_index(handle);
  require_true(
      join_status.code == KERNEL_ERROR_IO,
      "background rebuild join should surface rebuild failure as KERNEL_ERROR_IO");

  kernel_state_snapshot snapshot{};
  expect_ok(kernel_get_state(handle, &snapshot));
  require_true(snapshot.index_state == KERNEL_INDEX_UNAVAILABLE, "failed background rebuild should leave index_state UNAVAILABLE");

  expect_ok(kernel_export_diagnostics(handle, export_path.string().c_str()));
  const std::string exported = read_file_text(export_path);
  require_true(
      exported.find("\"index_fault_reason\":\"rebuild_failed\"") != std::string::npos,
      "background rebuild failure diagnostics should expose rebuild_failed");
  require_true(
      exported.find("\"rebuild_in_flight\":false") != std::string::npos,
      "background rebuild failure diagnostics should report no rebuild in flight after join");

  expect_ok(kernel_close(handle));
  std::filesystem::remove_all(vault);
  std::filesystem::remove_all(state_dir_for_vault(vault));
}

void test_background_rebuild_diagnostics_report_in_flight_while_running() {
  const auto vault = make_temp_vault();
  const auto export_path = vault / "diagnostics-background-rebuild-in-flight.json";
  kernel_handle* handle = nullptr;
  expect_ok(kernel_open_vault(vault.string().c_str(), &handle));
  require_index_ready(handle, "background rebuild diagnostics test should start from a ready state");

  kernel::index::inject_full_rescan_delay_ms(300, 1000);
  expect_ok(kernel_start_rebuild_index(handle));

  require_eventually(
      [&]() {
        kernel_state_snapshot snapshot{};
        return kernel_get_state(handle, &snapshot).code == KERNEL_OK &&
               snapshot.index_state == KERNEL_INDEX_REBUILDING;
      },
      "background rebuild diagnostics test should observe REBUILDING");

  expect_ok(kernel_export_diagnostics(handle, export_path.string().c_str()));
  const std::string exported = read_file_text(export_path);
  require_true(
      exported.find("\"index_state\":\"REBUILDING\"") != std::string::npos,
      "background rebuild diagnostics should report REBUILDING while work is in flight");
  require_true(
      exported.find("\"rebuild_in_flight\":true") != std::string::npos,
      "background rebuild diagnostics should report rebuild_in_flight=true while work is in flight");
  require_true(
      exported.find("\"rebuild_current_generation\":1") != std::string::npos,
      "background rebuild diagnostics should report the current rebuild generation while work is in flight");
  require_true(
      exported.find("\"rebuild_last_completed_generation\":0") != std::string::npos,
      "background rebuild diagnostics should preserve the last completed generation while the first rebuild is still running");
  require_true(
      exported.find("\"index_fault_reason\":\"\"") != std::string::npos,
      "background rebuild diagnostics should not report a live fault while rebuild is healthy");

  expect_ok(kernel_join_rebuild_index(handle));
  kernel::index::inject_full_rescan_delay_ms(0, 0);

  expect_ok(kernel_close(handle));
  std::filesystem::remove_all(vault);
  std::filesystem::remove_all(state_dir_for_vault(vault));
}

void test_sync_rebuild_rejects_while_background_rebuild_is_in_flight() {
  const auto vault = make_temp_vault();
  kernel_handle* handle = nullptr;
  expect_ok(kernel_open_vault(vault.string().c_str(), &handle));
  require_index_ready(handle, "sync rebuild conflict test should start from a ready state");

  kernel::index::inject_full_rescan_delay_ms(300, 1000);
  expect_ok(kernel_start_rebuild_index(handle));

  require_eventually(
      [&]() {
        kernel_state_snapshot snapshot{};
        return kernel_get_state(handle, &snapshot).code == KERNEL_OK &&
               snapshot.index_state == KERNEL_INDEX_REBUILDING;
      },
      "sync rebuild conflict test should observe background REBUILDING");

  const kernel_status sync_rebuild = kernel_rebuild_index(handle);
  require_true(
      sync_rebuild.code == KERNEL_ERROR_CONFLICT,
      "synchronous rebuild should reject while a background rebuild is already in flight");

  expect_ok(kernel_join_rebuild_index(handle));
  kernel::index::inject_full_rescan_delay_ms(0, 0);

  expect_ok(kernel_close(handle));
  std::filesystem::remove_all(vault);
  std::filesystem::remove_all(state_dir_for_vault(vault));
}

void test_background_rebuild_join_is_idempotent_after_completion() {
  const auto vault = make_temp_vault();
  kernel_handle* handle = nullptr;
  expect_ok(kernel_open_vault(vault.string().c_str(), &handle));
  require_index_ready(handle, "background rebuild join-idempotence test should start from a ready state");

  kernel::index::inject_full_rescan_delay_ms(300, 1000);
  expect_ok(kernel_start_rebuild_index(handle));
  expect_ok(kernel_join_rebuild_index(handle));
  kernel::index::inject_full_rescan_delay_ms(0, 0);

  const kernel_status second_join = kernel_join_rebuild_index(handle);
  require_true(
      second_join.code == KERNEL_OK,
      "joining a completed background rebuild a second time should remain a harmless no-op");

  kernel_state_snapshot snapshot{};
  expect_ok(kernel_get_state(handle, &snapshot));
  require_true(snapshot.index_state == KERNEL_INDEX_READY, "second join should leave runtime state READY");

  expect_ok(kernel_close(handle));
  std::filesystem::remove_all(vault);
  std::filesystem::remove_all(state_dir_for_vault(vault));
}

void test_background_rebuild_wait_times_out_while_work_is_in_flight() {
  const auto vault = make_temp_vault();
  kernel_handle* handle = nullptr;
  expect_ok(kernel_open_vault(vault.string().c_str(), &handle));
  require_index_ready(handle, "background rebuild wait-timeout test should start from a ready state");

  kernel::index::inject_full_rescan_delay_ms(300, 1000);
  expect_ok(kernel_start_rebuild_index(handle));

  const kernel_status wait_status = kernel_wait_for_rebuild(handle, 10);
  require_true(
      wait_status.code == KERNEL_ERROR_TIMEOUT,
      "background rebuild wait should report KERNEL_ERROR_TIMEOUT while delayed work is still in flight");

  kernel_state_snapshot snapshot{};
  expect_ok(kernel_get_state(handle, &snapshot));
  require_true(snapshot.index_state == KERNEL_INDEX_REBUILDING, "timed-out wait should leave runtime state REBUILDING");

  expect_ok(kernel_join_rebuild_index(handle));
  kernel::index::inject_full_rescan_delay_ms(0, 0);

  expect_ok(kernel_close(handle));
  std::filesystem::remove_all(vault);
  std::filesystem::remove_all(state_dir_for_vault(vault));
}

void test_background_rebuild_wait_returns_final_result_after_completion() {
  const auto vault = make_temp_vault();
  kernel_handle* handle = nullptr;
  expect_ok(kernel_open_vault(vault.string().c_str(), &handle));
  require_index_ready(handle, "background rebuild wait-success test should start from a ready state");

  kernel::index::inject_full_rescan_delay_ms(300, 1000);
  expect_ok(kernel_start_rebuild_index(handle));

  const kernel_status wait_status = kernel_wait_for_rebuild(handle, 5000);
  require_true(
      wait_status.code == KERNEL_OK,
      "background rebuild wait should return final success once rebuild completes within timeout");
  kernel::index::inject_full_rescan_delay_ms(0, 0);

  kernel_state_snapshot snapshot{};
  expect_ok(kernel_get_state(handle, &snapshot));
  require_true(snapshot.index_state == KERNEL_INDEX_READY, "successful background rebuild wait should leave runtime state READY");

  const kernel_status second_wait = kernel_wait_for_rebuild(handle, 1);
  require_true(
      second_wait.code == KERNEL_OK,
      "re-waiting a completed successful background rebuild should preserve the final result");

  const kernel_status join_after_wait = kernel_join_rebuild_index(handle);
  require_true(
      join_after_wait.code == KERNEL_OK,
      "joining after a successful wait should preserve the same final result");

  expect_ok(kernel_close(handle));
  std::filesystem::remove_all(vault);
  std::filesystem::remove_all(state_dir_for_vault(vault));
}

void test_background_rebuild_failure_result_remains_readable_after_completion() {
  const auto vault = make_temp_vault();
  kernel_handle* handle = nullptr;
  expect_ok(kernel_open_vault(vault.string().c_str(), &handle));
  require_index_ready(handle, "background rebuild failure retention test should start from a ready state");

  kernel::index::inject_full_rescan_failures(std::errc::io_error, 1);
  expect_ok(kernel_start_rebuild_index(handle));

  const kernel_status first_wait = kernel_wait_for_rebuild(handle, 5000);
  require_true(
      first_wait.code == KERNEL_ERROR_IO,
      "first wait should surface the background rebuild failure result");

  const kernel_status second_wait = kernel_wait_for_rebuild(handle, 1);
  require_true(
      second_wait.code == KERNEL_ERROR_IO,
      "re-waiting a completed failed background rebuild should preserve the failure result");

  const kernel_status join_after_wait = kernel_join_rebuild_index(handle);
  require_true(
      join_after_wait.code == KERNEL_ERROR_IO,
      "joining after a failed wait should preserve the same failure result");

  expect_ok(kernel_close(handle));
  std::filesystem::remove_all(vault);
  std::filesystem::remove_all(state_dir_for_vault(vault));
}

void test_close_waits_for_background_rebuild_to_finish_and_persist_result() {
  const auto vault = make_temp_vault();
  kernel_handle* handle = nullptr;
  expect_ok(kernel_open_vault(vault.string().c_str(), &handle));
  require_index_ready(handle, "close-during-background-rebuild test should start from a ready state");

  const std::string content =
      "# Close Rebuild Title\n"
      "close-rebuild-live-token\n";
  kernel_note_metadata metadata{};
  kernel_write_disposition disposition{};
  expect_ok(kernel_write_note(
      handle,
      "close-rebuild.md",
      content.data(),
      content.size(),
      nullptr,
      &metadata,
      &disposition));
  require_index_ready(handle, "seed write should settle before background rebuild close test");

  {
    std::lock_guard storage_lock(handle->storage_mutex);
    exec_sql(handle->storage.connection, "UPDATE notes SET title='Stale Close Title' WHERE rel_path='close-rebuild.md';");
    exec_sql(handle->storage.connection, "DELETE FROM note_fts WHERE rowid=(SELECT note_id FROM notes WHERE rel_path='close-rebuild.md');");
    exec_sql(
        handle->storage.connection,
        "INSERT INTO note_fts(rowid, title, body) VALUES("
        " (SELECT note_id FROM notes WHERE rel_path='close-rebuild.md'),"
        " 'Stale Close Title',"
        " 'close-rebuild-stale-token');");
  }

  kernel_search_results results{};
  expect_ok(kernel_search_notes(handle, "close-rebuild-stale-token", &results));
  require_true(results.count == 1, "stale FTS row should be visible before closing over background rebuild");
  kernel_free_search_results(&results);

  kernel::index::inject_full_rescan_delay_ms(300, 1000);
  expect_ok(kernel_start_rebuild_index(handle));
  expect_ok(kernel_close(handle));
  kernel::index::inject_full_rescan_delay_ms(0, 0);

  handle = nullptr;
  expect_ok(kernel_open_vault(vault.string().c_str(), &handle));
  require_index_ready(handle, "reopen after close-during-background-rebuild should settle to READY");

  expect_ok(kernel_search_notes(handle, "close-rebuild-stale-token", &results));
  require_true(results.count == 0, "close should wait for background rebuild so stale FTS rows are gone after reopen");
  kernel_free_search_results(&results);

  expect_ok(kernel_search_notes(handle, "close-rebuild-live-token", &results));
  require_true(results.count == 1, "close should preserve the completed background rebuild result after reopen");
  require_true(std::string(results.hits[0].rel_path) == "close-rebuild.md", "reopened rebuild result should preserve rel_path");
  kernel_free_search_results(&results);

  sqlite3* readonly_db = open_sqlite_readonly(storage_db_for_vault(vault));
  require_true(
      query_single_text(readonly_db, "SELECT title FROM notes WHERE rel_path='close-rebuild.md';") == "Close Rebuild Title",
      "close should not return until the background rebuild has restored disk truth");
  sqlite3_close(readonly_db);

  expect_ok(kernel_close(handle));
  std::filesystem::remove_all(vault);
  std::filesystem::remove_all(state_dir_for_vault(vault));
}

void test_background_rebuild_wait_and_join_report_not_found_when_no_task_exists() {
  const auto vault = make_temp_vault();
  kernel_handle* handle = nullptr;
  expect_ok(kernel_open_vault(vault.string().c_str(), &handle));
  require_index_ready(handle, "idle rebuild task test should start from a ready state");

  const kernel_status wait_status = kernel_wait_for_rebuild(handle, 1);
  require_true(
      wait_status.code == KERNEL_ERROR_NOT_FOUND,
      "background rebuild wait should report KERNEL_ERROR_NOT_FOUND when no rebuild task exists");

  const kernel_status join_status = kernel_join_rebuild_index(handle);
  require_true(
      join_status.code == KERNEL_ERROR_NOT_FOUND,
      "background rebuild join should report KERNEL_ERROR_NOT_FOUND when no rebuild task exists");

  expect_ok(kernel_close(handle));
  std::filesystem::remove_all(vault);
  std::filesystem::remove_all(state_dir_for_vault(vault));
}

void test_get_rebuild_status_reports_idle_then_running_then_success() {
  const auto vault = make_temp_vault();
  kernel_handle* handle = nullptr;
  expect_ok(kernel_open_vault(vault.string().c_str(), &handle));
  require_index_ready(handle, "rebuild status success test should start from a ready state");

  kernel_rebuild_status_snapshot rebuild_status{};
  expect_ok(kernel_get_rebuild_status(handle, &rebuild_status));
  require_true(!rebuild_status.in_flight, "fresh runtime should report no rebuild in flight");
  require_true(
      !rebuild_status.has_last_result,
      "fresh runtime should report has_last_result=false before any background rebuild completes");
  require_true(
      rebuild_status.last_result_code == KERNEL_ERROR_NOT_FOUND,
      "fresh runtime should report no completed background rebuild result");

  kernel::index::inject_full_rescan_delay_ms(300, 1000);
  expect_ok(kernel_start_rebuild_index(handle));

  require_eventually(
      [&]() {
        kernel_rebuild_status_snapshot during{};
        if (kernel_get_rebuild_status(handle, &during).code != KERNEL_OK) {
          return false;
        }
        return during.in_flight &&
               !during.has_last_result &&
               during.last_result_code == KERNEL_ERROR_NOT_FOUND;
      },
      "rebuild status should report an in-flight background rebuild without a completed result yet");

  expect_ok(kernel_wait_for_rebuild(handle, 5000));
  kernel::index::inject_full_rescan_delay_ms(0, 0);

  expect_ok(kernel_get_rebuild_status(handle, &rebuild_status));
  require_true(!rebuild_status.in_flight, "completed rebuild should no longer be in flight");
  require_true(rebuild_status.has_last_result, "completed rebuild should report has_last_result=true");
  require_true(rebuild_status.last_result_code == KERNEL_OK, "completed rebuild should report last result KERNEL_OK");

  expect_ok(kernel_close(handle));
  std::filesystem::remove_all(vault);
  std::filesystem::remove_all(state_dir_for_vault(vault));
}

void test_get_rebuild_status_reports_current_started_at_while_running() {
  const auto vault = make_temp_vault();
  kernel_handle* handle = nullptr;
  expect_ok(kernel_open_vault(vault.string().c_str(), &handle));
  require_index_ready(handle, "rebuild status current-start test should start from a ready state");

  kernel::index::inject_full_rescan_delay_ms(300, 1000);
  expect_ok(kernel_start_rebuild_index(handle));

  require_eventually(
      [&]() {
        kernel_rebuild_status_snapshot during{};
        if (kernel_get_rebuild_status(handle, &during).code != KERNEL_OK) {
          return false;
        }
        return during.in_flight && during.current_started_at_ns != 0;
      },
      "rebuild status should expose a non-zero current_started_at_ns while a background rebuild is running");

  expect_ok(kernel_wait_for_rebuild(handle, 5000));
  kernel::index::inject_full_rescan_delay_ms(0, 0);

  kernel_rebuild_status_snapshot after{};
  expect_ok(kernel_get_rebuild_status(handle, &after));
  require_true(!after.in_flight, "completed rebuild should no longer be in flight in current-start test");
  require_true(
      after.current_started_at_ns == 0,
      "completed rebuild should clear current_started_at_ns once no rebuild is in flight");
  require_true(after.last_result_at_ns != 0, "completed rebuild should still preserve the last result timestamp");

  expect_ok(kernel_close(handle));
  std::filesystem::remove_all(vault);
  std::filesystem::remove_all(state_dir_for_vault(vault));
}

void test_get_rebuild_status_reports_monotonic_task_generations() {
  const auto vault = make_temp_vault();
  kernel_handle* handle = nullptr;
  expect_ok(kernel_open_vault(vault.string().c_str(), &handle));
  require_index_ready(handle, "rebuild generation test should start from a ready state");

  kernel_rebuild_status_snapshot rebuild_status{};
  expect_ok(kernel_get_rebuild_status(handle, &rebuild_status));
  require_true(
      rebuild_status.current_generation == 0 && rebuild_status.last_completed_generation == 0,
      "fresh runtime should report zero current and completed rebuild generations");

  kernel::index::inject_full_rescan_delay_ms(300, 1000);
  expect_ok(kernel_start_rebuild_index(handle));

  require_eventually(
      [&]() {
        kernel_rebuild_status_snapshot during{};
        if (kernel_get_rebuild_status(handle, &during).code != KERNEL_OK) {
          return false;
        }
        return during.in_flight && during.current_generation == 1 && during.last_completed_generation == 0;
      },
      "first background rebuild should report current_generation=1 while running");

  expect_ok(kernel_wait_for_rebuild(handle, 5000));
  kernel::index::inject_full_rescan_delay_ms(0, 0);

  expect_ok(kernel_get_rebuild_status(handle, &rebuild_status));
  require_true(
      !rebuild_status.in_flight &&
          rebuild_status.current_generation == 0 &&
          rebuild_status.last_completed_generation == 1,
      "completed first rebuild should clear current_generation and retain last_completed_generation=1");

  kernel::index::inject_full_rescan_delay_ms(300, 1000);
  expect_ok(kernel_start_rebuild_index(handle));

  require_eventually(
      [&]() {
        kernel_rebuild_status_snapshot during{};
        if (kernel_get_rebuild_status(handle, &during).code != KERNEL_OK) {
          return false;
        }
        return during.in_flight && during.current_generation == 2 && during.last_completed_generation == 1;
      },
      "second background rebuild should advance current_generation while preserving the last completed generation");

  expect_ok(kernel_wait_for_rebuild(handle, 5000));
  kernel::index::inject_full_rescan_delay_ms(0, 0);

  expect_ok(kernel_get_rebuild_status(handle, &rebuild_status));
  require_true(
      !rebuild_status.in_flight &&
          rebuild_status.current_generation == 0 &&
          rebuild_status.last_completed_generation == 2,
      "completed second rebuild should advance last_completed_generation to 2");

  expect_ok(kernel_close(handle));
  std::filesystem::remove_all(vault);
  std::filesystem::remove_all(state_dir_for_vault(vault));
}

void test_get_rebuild_status_preserves_last_completed_result_while_next_task_runs() {
  const auto vault = make_temp_vault();
  kernel_handle* handle = nullptr;
  expect_ok(kernel_open_vault(vault.string().c_str(), &handle));
  require_index_ready(handle, "rebuild preserved-result test should start from a ready state");

  expect_ok(kernel_start_rebuild_index(handle));
  expect_ok(kernel_wait_for_rebuild(handle, 5000));

  kernel_rebuild_status_snapshot rebuild_status{};
  expect_ok(kernel_get_rebuild_status(handle, &rebuild_status));
  require_true(
      rebuild_status.has_last_result &&
          rebuild_status.last_result_code == KERNEL_OK &&
          rebuild_status.last_completed_generation == 1,
      "after one successful rebuild the status query should preserve the last completed success result");

  kernel::index::inject_full_rescan_delay_ms(300, 1000);
  expect_ok(kernel_start_rebuild_index(handle));

  require_eventually(
      [&]() {
        kernel_rebuild_status_snapshot during{};
        if (kernel_get_rebuild_status(handle, &during).code != KERNEL_OK) {
          return false;
        }
        return during.in_flight &&
               during.current_generation == 2 &&
               during.last_completed_generation == 1 &&
               during.has_last_result &&
               during.last_result_code == KERNEL_OK &&
               during.last_result_at_ns != 0;
      },
      "running a second rebuild should preserve the first rebuild's completed result while the new task is still in flight");

  expect_ok(kernel_wait_for_rebuild(handle, 5000));
  kernel::index::inject_full_rescan_delay_ms(0, 0);

  expect_ok(kernel_close(handle));
  std::filesystem::remove_all(vault);
  std::filesystem::remove_all(state_dir_for_vault(vault));
}

void test_get_rebuild_status_reports_background_failure_result() {
  const auto vault = make_temp_vault();
  kernel_handle* handle = nullptr;
  expect_ok(kernel_open_vault(vault.string().c_str(), &handle));
  require_index_ready(handle, "rebuild status failure test should start from a ready state");

  kernel::index::inject_full_rescan_failures(std::errc::io_error, 1);
  expect_ok(kernel_start_rebuild_index(handle));
  const kernel_status wait_status = kernel_wait_for_rebuild(handle, 5000);
  require_true(
      wait_status.code == KERNEL_ERROR_IO,
      "background rebuild wait should surface failure in rebuild status failure test");

  kernel_rebuild_status_snapshot rebuild_status{};
  expect_ok(kernel_get_rebuild_status(handle, &rebuild_status));
  require_true(!rebuild_status.in_flight, "failed rebuild should no longer be in flight");
  require_true(
      rebuild_status.has_last_result,
      "failed rebuild should still report has_last_result=true after a completed task");
  require_true(
      rebuild_status.last_result_code == KERNEL_ERROR_IO,
      "failed rebuild should report the last background rebuild result code");

  expect_ok(kernel_close(handle));
  std::filesystem::remove_all(vault);
  std::filesystem::remove_all(state_dir_for_vault(vault));
}


}  // namespace

void run_rebuild_runtime_tests() {
  test_rebuild_index_reconciles_disk_truth_after_db_drift();
  test_rebuild_reports_rebuilding_during_delayed_rescan();
  test_background_rebuild_start_reports_rebuilding_and_join_restores_ready();
  test_background_rebuild_rejects_duplicate_start_requests();
  test_background_rebuild_failure_surfaces_through_join_and_diagnostics();
  test_background_rebuild_diagnostics_report_in_flight_while_running();
  test_sync_rebuild_rejects_while_background_rebuild_is_in_flight();
  test_background_rebuild_join_is_idempotent_after_completion();
  test_background_rebuild_wait_times_out_while_work_is_in_flight();
  test_background_rebuild_wait_returns_final_result_after_completion();
  test_background_rebuild_failure_result_remains_readable_after_completion();
  test_close_waits_for_background_rebuild_to_finish_and_persist_result();
  test_background_rebuild_wait_and_join_report_not_found_when_no_task_exists();
  test_get_rebuild_status_reports_idle_then_running_then_success();
  test_get_rebuild_status_reports_current_started_at_while_running();
  test_get_rebuild_status_reports_monotonic_task_generations();
  test_get_rebuild_status_preserves_last_completed_result_while_next_task_runs();
  test_get_rebuild_status_reports_background_failure_result();
}
