// Reason: Keep core startup recovery cleanup and sidecar-truth regressions together so journal-tail tests can stay separate.

#include "kernel/c_api.h"

#include "api/kernel_api_runtime_recovery_startup_suites.h"
#include "api/kernel_api_test_support.h"
#include "recovery/journal.h"
#include "support/test_support.h"
#include "third_party/sqlite/sqlite3.h"

#include <filesystem>
#include <string>

namespace {

void test_open_vault_consumes_dangling_sidecar_recovery_record() {
  const auto vault = make_temp_vault();
  const auto state_dir = state_dir_for_vault(vault);
  const auto journal_path = journal_path_for_vault(vault);

  prepare_state_dir_for_vault(vault);

  require_true(
      kernel::recovery::append_save_begin(
          journal_path,
          "manual-op",
          "dangling.md",
          state_dir / "dangling.tmp")
              .value() == 0,
      "manual SAVE_BEGIN append should succeed");

  const auto db_path = storage_db_for_vault(vault);
  sqlite3* db = open_sqlite_readonly(db_path);
  require_true(
      query_single_int(db, "SELECT COUNT(*) FROM journal_state WHERE op_id='manual-op';") == 0,
      "journal_state mirror should stay empty for manual sidecar append");
  require_true(
      query_single_int(
          db,
          "SELECT COUNT(*) FROM ("
          " SELECT op_id"
          " FROM journal_state"
          " GROUP BY op_id"
          " HAVING SUM(CASE WHEN phase='SAVE_BEGIN' THEN 1 ELSE 0 END) > 0"
          "    AND SUM(CASE WHEN phase='SAVE_COMMIT' THEN 1 ELSE 0 END) = 0"
          ");") == 0,
      "sqlite unresolved recovery count should still be zero before reopen");
  sqlite3_close(db);

  kernel_handle* handle = nullptr;
  expect_ok(kernel_open_vault(vault.string().c_str(), &handle));
  require_index_ready(handle, "recovered startup note should settle before direct sqlite assertions");

  kernel_state_snapshot snapshot{};
  expect_ok(kernel_get_state(handle, &snapshot));
  require_true(snapshot.pending_recovery_ops == 0, "startup recovery should consume dangling sidecar record");
  expect_empty_journal_if_present(journal_path, "startup recovery should compact a consumed sidecar journal");

  expect_ok(kernel_close(handle));
  std::filesystem::remove_all(vault);
  std::filesystem::remove_all(state_dir);
}

void test_open_vault_recovers_unfinished_save_and_cleans_journal() {
  const auto vault = make_temp_vault();
  const auto state_dir = state_dir_for_vault(vault);
  const auto journal_path = journal_path_for_vault(vault);
  const auto target_path = vault / "recovered.md";
  const auto temp_path = target_path.parent_path() / "recovered.md.codex-recovery.tmp";

  prepare_state_dir_for_vault(vault);
  write_file_bytes(
      target_path,
      "# Recovered Title\n"
      "See [[Recovered Link]].\n"
      "#chem\n");
  write_file_bytes(temp_path, "stale-temp");

  require_true(
      kernel::recovery::append_save_begin(
          journal_path,
          "recovery-op",
          "recovered.md",
          temp_path)
              .value() == 0,
      "manual unfinished SAVE_BEGIN append should succeed");

  kernel_handle* handle = nullptr;
  expect_ok(kernel_open_vault(vault.string().c_str(), &handle));

  require_eventually(
      [&]() {
        kernel_state_snapshot snapshot{};
        return kernel_get_state(handle, &snapshot).code == KERNEL_OK &&
               snapshot.pending_recovery_ops == 0 &&
               snapshot.index_state == KERNEL_INDEX_READY &&
               snapshot.indexed_note_count == 1;
      },
      "startup recovery should settle to READY with recovered note metadata");
  require_true(!std::filesystem::exists(temp_path), "startup recovery should remove stale temp file");

  expect_empty_journal_if_present(journal_path, "startup recovery cleanup should leave an empty journal");

  sqlite3* db = open_sqlite_readonly(storage_db_for_vault(vault));
  require_true(
      query_single_int(db, "SELECT COUNT(*) FROM notes WHERE rel_path='recovered.md' AND is_deleted=0;") == 1,
      "recovered file should be reflected in notes after reopen");
  require_true(
      query_single_text(db, "SELECT title FROM notes WHERE rel_path='recovered.md';") == "Recovered Title",
      "recovered file should persist parser-derived title");
  require_true(
      query_single_int(
          db,
          "SELECT COUNT(*) FROM note_tags WHERE note_id=(SELECT note_id FROM notes WHERE rel_path='recovered.md');") ==
          1,
      "recovered file should persist parser-derived tags");
  require_true(
      query_single_int(
          db,
          "SELECT COUNT(*) FROM note_links WHERE note_id=(SELECT note_id FROM notes WHERE rel_path='recovered.md');") ==
          1,
      "recovered file should persist parser-derived wikilinks");
  sqlite3_close(db);

  expect_ok(kernel_close(handle));
  std::filesystem::remove_all(vault);
  std::filesystem::remove_all(state_dir);
}

void test_open_vault_discards_temp_only_unfinished_save() {
  const auto vault = make_temp_vault();
  const auto state_dir = state_dir_for_vault(vault);
  const auto journal_path = journal_path_for_vault(vault);
  const auto temp_path = vault / "orphaned.tmp";

  prepare_state_dir_for_vault(vault);
  write_file_bytes(temp_path, "orphan-temp");

  require_true(
      kernel::recovery::append_save_begin(
          journal_path,
          "orphan-op",
          "missing.md",
          temp_path)
              .value() == 0,
      "temp-only unfinished SAVE_BEGIN append should succeed");

  kernel_handle* handle = nullptr;
  expect_ok(kernel_open_vault(vault.string().c_str(), &handle));

  require_eventually(
      [&]() {
        kernel_state_snapshot snapshot{};
        return kernel_get_state(handle, &snapshot).code == KERNEL_OK &&
               snapshot.pending_recovery_ops == 0 &&
               snapshot.index_state == KERNEL_INDEX_READY &&
               snapshot.indexed_note_count == 0;
      },
      "temp-only unfinished save should settle to READY without creating a live note");
  require_true(!std::filesystem::exists(temp_path), "temp-only unfinished save should delete stale temp");

  sqlite3* db = open_sqlite_readonly(storage_db_for_vault(vault));
  require_true(
      query_single_int(db, "SELECT COUNT(*) FROM notes WHERE rel_path='missing.md' AND is_deleted=0;") == 0,
      "temp-only unfinished save must not synthesize note metadata");
  sqlite3_close(db);

  expect_ok(kernel_close(handle));
  std::filesystem::remove_all(vault);
  std::filesystem::remove_all(state_dir);
}

void test_get_state_ignores_sqlite_diagnostic_recovery_rows() {
  const auto vault = make_temp_vault();
  const auto db_path = storage_db_for_vault(vault);

  kernel_handle* handle = nullptr;
  expect_ok(kernel_open_vault(vault.string().c_str(), &handle));
  expect_ok(kernel_close(handle));

  sqlite3* db = nullptr;
  const int open_rc = sqlite3_open_v2(
      db_path.string().c_str(),
      &db,
      SQLITE_OPEN_READWRITE,
      nullptr);
  require_true(open_rc == SQLITE_OK, "sqlite database should open for write");
  exec_sql(
      db,
      "INSERT INTO journal_state(op_id, op_type, rel_path, temp_path, phase, recorded_at_ns) "
      "VALUES('diag-only-op', 'SAVE', 'diag.md', 'diag.tmp', 'SAVE_BEGIN', 1);");
  sqlite3_close(db);

  expect_ok(kernel_open_vault(vault.string().c_str(), &handle));

  kernel_state_snapshot snapshot{};
  expect_ok(kernel_get_state(handle, &snapshot));
  require_true(snapshot.pending_recovery_ops == 0, "sqlite diagnostic rows must not affect pending recovery");

  expect_ok(kernel_close(handle));
  std::filesystem::remove_all(vault);
  std::filesystem::remove_all(state_dir_for_vault(vault));
}

void test_startup_recovery_prefers_sidecar_truth_over_conflicting_journal_state_rows() {
  const auto vault = make_temp_vault();
  const auto state_dir = state_dir_for_vault(vault);
  const auto db_path = storage_db_for_vault(vault);
  const auto journal_path = journal_path_for_vault(vault);
  const auto target_path = vault / "sidecar-truth.md";
  const auto temp_path = target_path.parent_path() / "sidecar-truth.md.codex-recovery.tmp";

  prepare_state_dir_for_vault(vault);
  write_file_bytes(
      target_path,
      "# Sidecar Truth\n"
      "sidecar-truth-token\n");
  write_file_bytes(temp_path, "stale-temp");

  require_true(
      kernel::recovery::append_save_begin(
          journal_path,
          "sidecar-op",
          "sidecar-truth.md",
          temp_path)
              .value() == 0,
      "sidecar SAVE_BEGIN append should succeed before conflicting journal_state injection");

  sqlite3* db = open_sqlite_readwrite(db_path);
  exec_sql(
      db,
      "INSERT INTO journal_state(op_id, op_type, rel_path, temp_path, phase, recorded_at_ns) "
      "VALUES('sidecar-op', 'SAVE', 'sidecar-truth.md', '', 'SAVE_COMMIT', 1);");
  sqlite3_close(db);

  kernel_handle* handle = nullptr;
  expect_ok(kernel_open_vault(vault.string().c_str(), &handle));

  require_eventually(
      [&]() {
        kernel_state_snapshot snapshot{};
        return kernel_get_state(handle, &snapshot).code == KERNEL_OK &&
               snapshot.pending_recovery_ops == 0 &&
               snapshot.index_state == KERNEL_INDEX_READY &&
               snapshot.indexed_note_count == 1;
      },
      "startup recovery should still follow sidecar truth even when journal_state claims the op already committed");

  require_true(!std::filesystem::exists(temp_path), "sidecar-truth recovery should still remove stale temp");
  expect_empty_journal_if_present(journal_path, "sidecar-truth recovery should compact the consumed sidecar journal");

  db = open_sqlite_readonly(db_path);
  require_true(
      query_single_int(db, "SELECT COUNT(*) FROM notes WHERE rel_path='sidecar-truth.md' AND is_deleted=0;") == 1,
      "sidecar truth should still recover the live note even if journal_state contains conflicting rows");
  sqlite3_close(db);

  expect_ok(kernel_close(handle));
  std::filesystem::remove_all(vault);
  std::filesystem::remove_all(state_dir);
}

}  // namespace

void run_runtime_recovery_startup_cleanup_tests() {
  test_open_vault_consumes_dangling_sidecar_recovery_record();
  test_open_vault_recovers_unfinished_save_and_cleans_journal();
  test_open_vault_discards_temp_only_unfinished_save();
  test_get_state_ignores_sqlite_diagnostic_recovery_rows();
  test_startup_recovery_prefers_sidecar_truth_over_conflicting_journal_state_rows();
}
