// Reason: This file isolates core state and schema contract tests so write semantics can live separately.

#include "kernel/c_api.h"

#include "api/kernel_api_core_base_suites.h"
#include "api/kernel_api_test_support.h"
#include "index/refresh.h"
#include "support/test_support.h"
#include "third_party/sqlite/sqlite3.h"

#include <filesystem>
#include <string>

namespace {

void test_open_and_state_layers() {
  const auto vault = make_temp_vault();
  kernel_handle* handle = nullptr;

  expect_ok(kernel_open_vault(vault.string().c_str(), &handle));

  kernel_state_snapshot snapshot{};
  expect_ok(kernel_get_state(handle, &snapshot));
  require_true(snapshot.session_state == KERNEL_SESSION_OPEN, "session state should be OPEN");
  require_true(
      snapshot.index_state == KERNEL_INDEX_CATCHING_UP ||
          snapshot.index_state == KERNEL_INDEX_READY,
      "index state should start in CATCHING_UP and eventually reach READY");

  expect_ok(kernel_close(handle));
  std::filesystem::remove_all(vault);
}

void test_index_state_reports_catching_up_during_delayed_initial_scan() {
  const auto vault = make_temp_vault();
  kernel::index::inject_full_rescan_delay_ms(300, 1);

  kernel_handle* handle = nullptr;
  expect_ok(kernel_open_vault(vault.string().c_str(), &handle));

  require_eventually(
      [&]() {
        kernel_state_snapshot snapshot{};
        return kernel_get_state(handle, &snapshot).code == KERNEL_OK &&
               snapshot.index_state == KERNEL_INDEX_CATCHING_UP;
      },
      "delayed initial scan should expose CATCHING_UP to the host");

  require_index_ready(handle, "delayed initial scan should eventually settle back to READY");

  expect_ok(kernel_close(handle));
  std::filesystem::remove_all(vault);
}

void test_catching_up_hides_stale_indexed_count_until_reconciled() {
  const auto vault = make_temp_vault();
  kernel_handle* handle = nullptr;
  expect_ok(kernel_open_vault(vault.string().c_str(), &handle));

  const std::string original =
      "# Stale Count Title\n"
      "stale-count-token\n";
  kernel_note_metadata metadata{};
  kernel_write_disposition disposition{};
  expect_ok(kernel_write_note(
      handle,
      "stale-count.md",
      original.data(),
      original.size(),
      nullptr,
      &metadata,
      &disposition));
  require_index_ready(handle, "seed write should settle before close");
  expect_ok(kernel_close(handle));

  std::filesystem::remove(vault / "stale-count.md");
  kernel::index::inject_full_rescan_delay_ms(300, 1);

  expect_ok(kernel_open_vault(vault.string().c_str(), &handle));

  require_eventually(
      [&]() {
        kernel_state_snapshot snapshot{};
        return kernel_get_state(handle, &snapshot).code == KERNEL_OK &&
               snapshot.index_state == KERNEL_INDEX_CATCHING_UP &&
               snapshot.indexed_note_count == 0;
      },
      "CATCHING_UP should hide stale indexed counts until reconciliation completes");

  require_eventually(
      [&]() {
        kernel_state_snapshot snapshot{};
        return kernel_get_state(handle, &snapshot).code == KERNEL_OK &&
               snapshot.index_state == KERNEL_INDEX_READY &&
               snapshot.indexed_note_count == 0;
      },
      "catch-up should settle to READY with zero live notes after delete reconciliation");

  kernel::index::inject_full_rescan_delay_ms(0, 0);
  expect_ok(kernel_close(handle));
  std::filesystem::remove_all(vault);
  std::filesystem::remove_all(state_dir_for_vault(vault));
}

void test_open_creates_state_dir_and_storage_db() {
  const auto vault = make_temp_vault();
  const auto state_dir = state_dir_for_vault(vault);
  const auto db_path = storage_db_for_vault(vault);
  std::filesystem::remove_all(state_dir);

  kernel_handle* handle = nullptr;
  expect_ok(kernel_open_vault(vault.string().c_str(), &handle));
  require_index_ready(handle, "open_vault should finish initial catch-up before schema assertions");
  require_true(std::filesystem::is_directory(state_dir), "open_vault should create state_dir");
  require_true(std::filesystem::is_regular_file(db_path), "open_vault should create state sqlite db");

  sqlite3* db = open_sqlite_readonly(db_path);
  require_true(query_single_int(db, "PRAGMA user_version;") == 8, "storage schema should be at version 8");
  require_true(
      query_single_int(db, "SELECT COUNT(*) FROM sqlite_master WHERE type='table' AND name='schema_meta';") == 1,
      "schema_meta table should exist");
  require_true(
      query_single_int(db, "SELECT COUNT(*) FROM sqlite_master WHERE type='table' AND name='notes';") == 1,
      "notes table should exist");
  require_true(
      query_single_int(db, "SELECT COUNT(*) FROM sqlite_master WHERE type='table' AND name='journal_state';") == 1,
      "journal_state table should exist");
  require_true(
      query_single_int(db, "SELECT COUNT(*) FROM sqlite_master WHERE type='table' AND name='note_tags';") == 1,
      "note_tags table should exist");
  require_true(
      query_single_int(db, "SELECT COUNT(*) FROM sqlite_master WHERE type='table' AND name='note_links';") == 1,
      "note_links table should exist");
  require_true(
      query_single_int(db, "SELECT COUNT(*) FROM sqlite_master WHERE type='table' AND name='attachments';") == 1,
      "attachments table should exist");
  require_true(
      query_single_int(db, "SELECT COUNT(*) FROM sqlite_master WHERE type='table' AND name='note_attachment_refs';") == 1,
      "note_attachment_refs table should exist");
  require_true(
      query_single_int(db, "SELECT COUNT(*) FROM sqlite_master WHERE type='table' AND name='note_pdf_source_refs';") == 1,
      "note_pdf_source_refs table should exist");
  require_true(
      query_single_int(db, "SELECT COUNT(*) FROM sqlite_master WHERE type='table' AND name='note_chem_spectrum_refs';") == 1,
      "note_chem_spectrum_refs table should exist");
  require_true(
      query_single_int(db, "SELECT COUNT(*) FROM sqlite_master WHERE type='table' AND name='note_fts';") == 1,
      "note_fts table should exist");
  sqlite3_close(db);

  expect_ok(kernel_close(handle));
  std::filesystem::remove_all(vault);
  std::filesystem::remove_all(state_dir);
}

void test_get_state_reflects_persisted_notes_and_recovery_queue() {
  const auto vault = make_temp_vault();
  kernel_handle* handle = nullptr;
  expect_ok(kernel_open_vault(vault.string().c_str(), &handle));

  kernel_note_metadata metadata{};
  kernel_write_disposition disposition{};
  const std::string content = "state";
  expect_ok(kernel_write_note(
      handle,
      "state.md",
      content.data(),
      content.size(),
      nullptr,
      &metadata,
      &disposition));

  kernel_state_snapshot snapshot{};
  expect_ok(kernel_get_state(handle, &snapshot));
  require_true(snapshot.indexed_note_count == 1, "state should report one persisted note");
  require_true(snapshot.pending_recovery_ops == 0, "state should report no pending recovery ops after commit");

  expect_ok(kernel_close(handle));
  std::filesystem::remove_all(vault);
}

void test_open_vault_reopen_preserves_schema_v1() {
  const auto vault = make_temp_vault();
  const auto db_path = storage_db_for_vault(vault);

  kernel_handle* handle = nullptr;
  expect_ok(kernel_open_vault(vault.string().c_str(), &handle));
  expect_ok(kernel_close(handle));

  expect_ok(kernel_open_vault(vault.string().c_str(), &handle));
  expect_ok(kernel_close(handle));

  sqlite3* db = open_sqlite_readonly(db_path);
  require_true(query_single_int(db, "PRAGMA user_version;") == 8, "schema reopen should preserve user_version=8");
  require_true(
      query_single_text(db, "SELECT value FROM schema_meta WHERE key='schema_version';") == "8",
      "schema reopen should preserve schema_meta version");
  require_true(
      query_single_int(db, "SELECT COUNT(*) FROM notes WHERE is_deleted=0;") == 0,
      "schema reopen should remain idempotent for an untouched vault");
  require_true(
      query_single_int(db, "SELECT COUNT(*) FROM sqlite_master WHERE type='table' AND name='note_tags';") == 1,
      "schema reopen should preserve note_tags table");
  require_true(
      query_single_int(db, "SELECT COUNT(*) FROM sqlite_master WHERE type='table' AND name='note_links';") == 1,
      "schema reopen should preserve note_links table");
  require_true(
      query_single_int(db, "SELECT COUNT(*) FROM sqlite_master WHERE type='table' AND name='note_fts';") == 1,
      "schema reopen should preserve note_fts table");
  require_true(
      query_single_int(db, "SELECT COUNT(*) FROM sqlite_master WHERE type='table' AND name='note_pdf_source_refs';") == 1,
      "schema reopen should preserve note_pdf_source_refs table");
  require_true(
      query_single_int(db, "SELECT COUNT(*) FROM sqlite_master WHERE type='table' AND name='note_chem_spectrum_refs';") == 1,
      "schema reopen should preserve note_chem_spectrum_refs table");
  sqlite3_close(db);

  std::filesystem::remove_all(vault);
  std::filesystem::remove_all(state_dir_for_vault(vault));
}

}  // namespace

void run_kernel_api_core_state_contract_tests() {
  test_open_and_state_layers();
  test_index_state_reports_catching_up_during_delayed_initial_scan();
  test_catching_up_hides_stale_indexed_count_until_reconciled();
  test_open_creates_state_dir_and_storage_db();
  test_get_state_reflects_persisted_notes_and_recovery_queue();
  test_open_vault_reopen_preserves_schema_v1();
}
