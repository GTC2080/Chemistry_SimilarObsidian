// Reason: This file isolates background rebuild lifecycle, conflict, and diagnostics coverage so wait semantics can stay separate.

#include "kernel/c_api.h"

#include "api/kernel_api_rebuild_runtime_background_suites.h"
#include "api/kernel_api_test_support.h"
#include "core/kernel_internal.h"
#include "index/refresh.h"
#include "support/test_support.h"
#include "third_party/sqlite/sqlite3.h"

#include <filesystem>
#include <mutex>
#include <string>
#include <system_error>

namespace {

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

}  // namespace

void run_rebuild_runtime_background_lifecycle_tests() {
  test_background_rebuild_start_reports_rebuilding_and_join_restores_ready();
  test_background_rebuild_rejects_duplicate_start_requests();
  test_background_rebuild_failure_surfaces_through_join_and_diagnostics();
  test_background_rebuild_diagnostics_report_in_flight_while_running();
  test_sync_rebuild_rejects_while_background_rebuild_is_in_flight();
  test_close_waits_for_background_rebuild_to_finish_and_persist_result();
}
