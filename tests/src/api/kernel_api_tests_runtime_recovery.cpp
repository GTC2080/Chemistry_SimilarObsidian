// Reason: This file isolates non-attachment runtime recovery and watcher-backoff regressions so the main API suite can stay focused on broader ABI coverage.

#include "kernel/c_api.h"

#include "api/kernel_api_test_support.h"
#include "core/kernel_internal.h"
#include "index/refresh.h"
#include "recovery/journal.h"
#include "storage/storage.h"
#include "support/test_support.h"
#include "third_party/sqlite/sqlite3.h"
#include "watcher/integration.h"
#include "watcher/session.h"

#include <filesystem>
#include <mutex>
#include <string>
#include <system_error>
#include <thread>
#include <vector>

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

void test_open_vault_ignores_torn_tail_after_valid_recovery_prefix() {
  const auto vault = make_temp_vault();
  const auto state_dir = state_dir_for_vault(vault);
  const auto journal_path = journal_path_for_vault(vault);
  const auto target_path = vault / "tail.md";
  const auto temp_path = target_path.parent_path() / "tail.md.codex-recovery.tmp";

  prepare_state_dir_for_vault(vault);
  write_file_bytes(target_path, "tail-body");
  write_file_bytes(temp_path, "tail-temp");

  require_true(
      kernel::recovery::append_save_begin(
          journal_path,
          "tail-op",
          "tail.md",
          temp_path)
          .value() == 0,
      "valid SAVE_BEGIN append should succeed before torn tail injection");
  append_raw_bytes(journal_path, "BROKEN");

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
      "valid prefix recovery should settle to READY after torn-tail cleanup");
  require_true(!std::filesystem::exists(temp_path), "torn-tail recovery should still remove stale temp");

  expect_empty_journal_if_present(journal_path, "torn-tail recovery should compact journal to empty valid prefix");

  sqlite3* db = open_sqlite_readonly(storage_db_for_vault(vault));
  require_true(
      query_single_int(db, "SELECT COUNT(*) FROM notes WHERE rel_path='tail.md' AND is_deleted=0;") == 1,
      "valid prefix recovery should persist the recovered note");
  sqlite3_close(db);

  expect_ok(kernel_close(handle));
  std::filesystem::remove_all(vault);
  std::filesystem::remove_all(state_dir);
}

void test_open_vault_ignores_truncated_tail_after_valid_recovery_prefix() {
  const auto vault = make_temp_vault();
  const auto state_dir = state_dir_for_vault(vault);
  const auto journal_path = journal_path_for_vault(vault);
  const auto target_path = vault / "truncated.md";
  const auto temp_path = target_path.parent_path() / "truncated.md.codex-recovery.tmp";

  prepare_state_dir_for_vault(vault);
  write_file_bytes(target_path, "truncated-body");
  write_file_bytes(temp_path, "truncated-temp");

  require_true(
      kernel::recovery::append_save_begin(
          journal_path,
          "truncated-op",
          "truncated.md",
          temp_path)
          .value() == 0,
      "valid SAVE_BEGIN append should succeed before truncated tail injection");
  append_truncated_tail_record(journal_path);

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
      "valid prefix recovery should settle to READY after truncated-tail cleanup");
  require_true(!std::filesystem::exists(temp_path), "truncated-tail recovery should still remove stale temp");

  expect_empty_journal_if_present(
      journal_path,
      "truncated-tail recovery should compact journal to empty valid prefix");

  expect_ok(kernel_close(handle));
  std::filesystem::remove_all(vault);
  std::filesystem::remove_all(state_dir);
}

void test_open_vault_ignores_crc_mismatch_tail_after_valid_recovery_prefix() {
  const auto vault = make_temp_vault();
  const auto state_dir = state_dir_for_vault(vault);
  const auto journal_path = journal_path_for_vault(vault);
  const auto target_path = vault / "crc.md";
  const auto temp_path = target_path.parent_path() / "crc.md.codex-recovery.tmp";

  prepare_state_dir_for_vault(vault);
  write_file_bytes(target_path, "crc-body");
  write_file_bytes(temp_path, "crc-temp");

  require_true(
      kernel::recovery::append_save_begin(
          journal_path,
          "crc-op",
          "crc.md",
          temp_path)
          .value() == 0,
      "valid SAVE_BEGIN append should succeed before CRC tail injection");
  append_crc_mismatch_tail_record(journal_path);

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
      "valid prefix recovery should settle to READY after CRC-tail cleanup");
  require_true(!std::filesystem::exists(temp_path), "crc mismatch recovery should still remove stale temp");

  expect_empty_journal_if_present(journal_path, "crc mismatch recovery should compact journal to empty valid prefix");

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

void test_startup_recovery_before_target_replace_keeps_old_disk_truth() {
  const auto vault = make_temp_vault();
  const auto state_dir = state_dir_for_vault(vault);
  const auto db_path = storage_db_for_vault(vault);
  const auto journal_path = journal_path_for_vault(vault);
  const auto target_path = vault / "before-replace.md";
  const auto temp_path = target_path.parent_path() / "before-replace.md.codex-recovery.tmp";

  kernel_handle* handle = nullptr;
  expect_ok(kernel_open_vault(vault.string().c_str(), &handle));

  const std::string original_content =
      "# Before Replace Title\n"
      "before-replace-live-token\n"
      "#beforetag\n"
      "[[BeforeLink]]\n";
  kernel_note_metadata metadata{};
  kernel_write_disposition disposition{};
  expect_ok(kernel_write_note(
      handle,
      "before-replace.md",
      original_content.data(),
      original_content.size(),
      nullptr,
      &metadata,
      &disposition));
  require_index_ready(handle, "seed write should settle before manual pre-replace recovery setup");
  expect_ok(kernel_close(handle));

  sqlite3* db = open_sqlite_readwrite(db_path);
  exec_sql(db, "UPDATE notes SET title='Temp Stage Title' WHERE rel_path='before-replace.md';");
  exec_sql(db, "DELETE FROM note_tags WHERE note_id=(SELECT note_id FROM notes WHERE rel_path='before-replace.md');");
  exec_sql(
      db,
      "INSERT INTO note_tags(note_id, tag) "
      "VALUES((SELECT note_id FROM notes WHERE rel_path='before-replace.md'), 'tempstagetag');");
  exec_sql(db, "DELETE FROM note_links WHERE note_id=(SELECT note_id FROM notes WHERE rel_path='before-replace.md');");
  exec_sql(
      db,
      "INSERT INTO note_links(note_id, target) "
      "VALUES((SELECT note_id FROM notes WHERE rel_path='before-replace.md'), 'TempStageLink');");
  exec_sql(db, "DELETE FROM note_fts WHERE rowid=(SELECT note_id FROM notes WHERE rel_path='before-replace.md');");
  exec_sql(
      db,
      "INSERT INTO note_fts(rowid, title, body) VALUES("
      " (SELECT note_id FROM notes WHERE rel_path='before-replace.md'),"
      " 'Temp Stage Title',"
      " 'temp-stage-token');");
  sqlite3_close(db);

  write_file_bytes(
      temp_path,
      "# Temp Stage Title\n"
      "temp-stage-token\n"
      "#tempstagetag\n"
      "[[TempStageLink]]\n");
  require_true(
      kernel::recovery::append_save_begin(
          journal_path,
          "before-replace-op",
          "before-replace.md",
          temp_path)
          .value() == 0,
      "manual SAVE_BEGIN should succeed for pre-replace crash simulation");

  expect_ok(kernel_open_vault(vault.string().c_str(), &handle));

  require_eventually(
      [&]() {
        kernel_state_snapshot snapshot{};
        if (kernel_get_state(handle, &snapshot).code != KERNEL_OK ||
            snapshot.pending_recovery_ops != 0 ||
            snapshot.index_state != KERNEL_INDEX_READY) {
          return false;
        }

        kernel_search_results stale_results{};
        if (kernel_search_notes(handle, "temp-stage-token", &stale_results).code != KERNEL_OK) {
          return false;
        }
        const bool stale_gone = stale_results.count == 0;
        kernel_free_search_results(&stale_results);

        kernel_search_results live_results{};
        if (kernel_search_notes(handle, "before-replace-live-token", &live_results).code != KERNEL_OK) {
          return false;
        }
        const bool live_present =
            live_results.count == 1 &&
            std::string(live_results.hits[0].rel_path) == "before-replace.md";
        kernel_free_search_results(&live_results);
        return stale_gone && live_present;
      },
      "startup recovery before target replace should keep old disk truth instead of indexing temp content");

  require_true(!std::filesystem::exists(temp_path), "pre-replace recovery should remove the staged temp file");
  expect_empty_journal_if_present(journal_path, "pre-replace recovery should compact the consumed journal");

  db = open_sqlite_readonly(db_path);
  require_true(
      query_single_text(db, "SELECT title FROM notes WHERE rel_path='before-replace.md';") ==
          "Before Replace Title",
      "pre-replace recovery should preserve the old target title");
  require_true(
      query_single_text(
          db,
          "SELECT tag FROM note_tags WHERE note_id=(SELECT note_id FROM notes WHERE rel_path='before-replace.md') LIMIT 1;") ==
          "beforetag",
      "pre-replace recovery should preserve old parser-derived tags");
  require_true(
      query_single_text(
          db,
          "SELECT target FROM note_links WHERE note_id=(SELECT note_id FROM notes WHERE rel_path='before-replace.md') LIMIT 1;") ==
          "BeforeLink",
      "pre-replace recovery should preserve old parser-derived links");
  sqlite3_close(db);

  expect_ok(kernel_close(handle));
  std::filesystem::remove_all(vault);
  std::filesystem::remove_all(state_dir);
}

void test_startup_recovery_after_temp_cleanup_recovers_replaced_target_truth() {
  const auto vault = make_temp_vault();
  const auto state_dir = state_dir_for_vault(vault);
  const auto db_path = storage_db_for_vault(vault);
  const auto journal_path = journal_path_for_vault(vault);
  const auto target_path = vault / "post-replace.md";
  const auto temp_path = target_path.parent_path() / "post-replace.md.codex-recovery.tmp";

  kernel_handle* handle = nullptr;
  expect_ok(kernel_open_vault(vault.string().c_str(), &handle));

  const std::string original_content =
      "# Original Replace Title\n"
      "post-replace-old-token\n"
      "#oldreplace\n"
      "[[OldReplaceLink]]\n";
  kernel_note_metadata metadata{};
  kernel_write_disposition disposition{};
  expect_ok(kernel_write_note(
      handle,
      "post-replace.md",
      original_content.data(),
      original_content.size(),
      nullptr,
      &metadata,
      &disposition));
  require_index_ready(handle, "seed write should settle before manual post-replace recovery setup");
  expect_ok(kernel_close(handle));

  write_file_bytes(
      target_path,
      "# Replaced Target Title\n"
      "post-replace-new-token\n"
      "#newreplace\n"
      "[[NewReplaceLink]]\n");
  require_true(
      !std::filesystem::exists(temp_path),
      "post-replace crash simulation should start with temp already gone");
  require_true(
      kernel::recovery::append_save_begin(
          journal_path,
          "post-replace-op",
          "post-replace.md",
          temp_path)
          .value() == 0,
      "manual SAVE_BEGIN should succeed for post-replace recovery simulation");

  expect_ok(kernel_open_vault(vault.string().c_str(), &handle));

  require_eventually(
      [&]() {
        kernel_state_snapshot snapshot{};
        if (kernel_get_state(handle, &snapshot).code != KERNEL_OK ||
            snapshot.pending_recovery_ops != 0 ||
            snapshot.index_state != KERNEL_INDEX_READY) {
          return false;
        }

        kernel_search_results stale_results{};
        if (kernel_search_notes(handle, "post-replace-old-token", &stale_results).code != KERNEL_OK) {
          return false;
        }
        const bool stale_gone = stale_results.count == 0;
        kernel_free_search_results(&stale_results);

        kernel_search_results live_results{};
        if (kernel_search_notes(handle, "post-replace-new-token", &live_results).code != KERNEL_OK) {
          return false;
        }
        const bool live_present =
            live_results.count == 1 &&
            std::string(live_results.hits[0].rel_path) == "post-replace.md";
        kernel_free_search_results(&live_results);
        return stale_gone && live_present;
      },
      "startup recovery after temp cleanup should recover the already-replaced target file");

  require_true(
      !std::filesystem::exists(temp_path),
      "post-replace recovery should tolerate an already-cleaned temp path");
  expect_empty_journal_if_present(journal_path, "post-replace recovery should compact the consumed journal");

  sqlite3* db = open_sqlite_readonly(db_path);
  require_true(
      query_single_text(db, "SELECT title FROM notes WHERE rel_path='post-replace.md';") ==
          "Replaced Target Title",
      "post-replace recovery should reindex the replaced target title");
  require_true(
      query_single_text(
          db,
          "SELECT tag FROM note_tags WHERE note_id=(SELECT note_id FROM notes WHERE rel_path='post-replace.md') LIMIT 1;") ==
          "newreplace",
      "post-replace recovery should reindex new parser-derived tags");
  require_true(
      query_single_text(
          db,
          "SELECT target FROM note_links WHERE note_id=(SELECT note_id FROM notes WHERE rel_path='post-replace.md') LIMIT 1;") ==
          "NewReplaceLink",
      "post-replace recovery should reindex new parser-derived links");
  sqlite3_close(db);

  expect_ok(kernel_close(handle));
  std::filesystem::remove_all(vault);
  std::filesystem::remove_all(state_dir);
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

void test_startup_recovery_replaces_stale_parser_derived_rows() {
  const auto vault = make_temp_vault();
  const auto db_path = storage_db_for_vault(vault);
  const auto state_dir = state_dir_for_vault(vault);
  const auto journal_path = journal_path_for_vault(vault);
  const auto target_path = vault / "recover-rewrite.md";
  const auto temp_path = target_path.parent_path() / "recover-rewrite.md.tmp";

  kernel_handle* handle = nullptr;
  expect_ok(kernel_open_vault(vault.string().c_str(), &handle));

  const std::string original =
      "# Before Recovery\n"
      "See [[OldLink]].\n"
      "Tags: #oldtag\n";
  kernel_note_metadata metadata{};
  kernel_write_disposition disposition{};
  expect_ok(kernel_write_note(
      handle,
      "recover-rewrite.md",
      original.data(),
      original.size(),
      nullptr,
      &metadata,
      &disposition));
  expect_ok(kernel_close(handle));

  write_file_bytes(
      target_path,
      "# After Recovery\n"
      "See [[NewLink]].\n"
      "Tags: #newtag\n");
  write_file_bytes(temp_path, "stale-temp");
  require_true(
      kernel::recovery::append_save_begin(
          journal_path,
          "recover-rewrite-op",
          "recover-rewrite.md",
          temp_path)
          .value() == 0,
      "recovery rewrite SAVE_BEGIN append should succeed");

  expect_ok(kernel_open_vault(vault.string().c_str(), &handle));

  sqlite3* db = open_sqlite_readonly(db_path);
  require_true(
      query_single_text(db, "SELECT title FROM notes WHERE rel_path='recover-rewrite.md';") == "After Recovery",
      "startup recovery should replace stale title");
  require_true(
      query_single_int(
          db,
          "SELECT COUNT(*) FROM note_tags WHERE note_id=(SELECT note_id FROM notes WHERE rel_path='recover-rewrite.md');") ==
          1,
      "startup recovery should clear stale tags before inserting new ones");
  require_true(
      query_single_text(
          db,
          "SELECT tag FROM note_tags WHERE note_id=(SELECT note_id FROM notes WHERE rel_path='recover-rewrite.md') LIMIT 1;") ==
          "newtag",
      "startup recovery should persist only the recovered tag set");
  require_true(
      query_single_int(
          db,
          "SELECT COUNT(*) FROM note_links WHERE note_id=(SELECT note_id FROM notes WHERE rel_path='recover-rewrite.md');") ==
          1,
      "startup recovery should clear stale wikilinks before inserting new ones");
  require_true(
      query_single_text(
          db,
          "SELECT target FROM note_links WHERE note_id=(SELECT note_id FROM notes WHERE rel_path='recover-rewrite.md') LIMIT 1;") ==
          "NewLink",
      "startup recovery should persist only the recovered wikilink set");
  sqlite3_close(db);

  expect_ok(kernel_close(handle));
  std::filesystem::remove_all(vault);
  std::filesystem::remove_all(state_dir);
}

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
