// Reason: This file isolates non-attachment runtime recovery and watcher-backoff regressions so the main API suite can stay focused on broader ABI coverage.

#include "kernel/c_api.h"

#include "api/kernel_api_runtime_recovery_interruptions.h"
#include "api/kernel_api_test_support.h"
#include "core/kernel_internal.h"
#include "recovery/journal.h"
#include "support/test_support.h"
#include "third_party/sqlite/sqlite3.h"

#include <filesystem>
#include <mutex>
#include <string>

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

void run_runtime_recovery_tests() {
  test_open_vault_catches_up_external_modify_while_closed();
  test_open_vault_consumes_dangling_sidecar_recovery_record();
  test_open_vault_recovers_unfinished_save_and_cleans_journal();
  test_open_vault_ignores_torn_tail_after_valid_recovery_prefix();
  test_open_vault_ignores_truncated_tail_after_valid_recovery_prefix();
  test_open_vault_ignores_crc_mismatch_tail_after_valid_recovery_prefix();
  test_open_vault_discards_temp_only_unfinished_save();
  test_get_state_ignores_sqlite_diagnostic_recovery_rows();
  test_startup_recovery_prefers_sidecar_truth_over_conflicting_journal_state_rows();
  test_startup_recovery_before_target_replace_keeps_old_disk_truth();
  test_startup_recovery_after_temp_cleanup_recovers_replaced_target_truth();
  test_reopen_catch_up_repairs_stale_derived_state_left_by_interrupted_rebuild();
  test_startup_recovery_replaces_stale_parser_derived_rows();
  test_close_during_watcher_fault_backoff_leaves_delete_for_reopen_catch_up();
  test_close_during_watcher_fault_backoff_leaves_modify_for_reopen_catch_up();
  test_close_during_watcher_fault_backoff_leaves_create_for_reopen_catch_up();
  test_reopen_catch_up_repairs_partial_state_left_by_interrupted_background_rebuild();
  test_reopen_catch_up_repairs_partial_state_left_by_interrupted_watcher_apply();
}
