// Reason: This file locks the first observable C ABI behaviors before internal modules grow.

#include "kernel/c_api.h"
#include "api/kernel_api_test_support.h"
#include "core/kernel_internal.h"
#include "core/kernel_shared.h"
#include "index/refresh.h"
#include "recovery/journal.h"
#include "search/search.h"
#include "support/test_support.h"
#include "third_party/sqlite/sqlite3.h"
#include "watcher/integration.h"
#include "watcher/session.h"

#include <filesystem>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <string>
#include <thread>

namespace {

std::size_t count_occurrences(const std::string& haystack, const std::string& needle) {
  if (needle.empty()) {
    return 0;
  }

  std::size_t count = 0;
  std::size_t offset = 0;
  while ((offset = haystack.find(needle, offset)) != std::string::npos) {
    ++count;
    offset += needle.size();
  }
  return count;
}

std::string two_digit_index(const int value) {
  if (value < 10) {
    return "0" + std::to_string(value);
  }
  return std::to_string(value);
}

struct AttachmentAnomalySnapshot {
  int missing_attachment_count = 0;
  int orphaned_attachment_count = 0;
  std::string summary = "clean";
};

std::string summarize_attachment_anomalies(
    const int missing_attachment_count,
    const int orphaned_attachment_count) {
  if (missing_attachment_count != 0 && orphaned_attachment_count != 0) {
    return "missing_live_and_orphaned_attachments";
  }
  if (missing_attachment_count != 0) {
    return "missing_live_attachments";
  }
  if (orphaned_attachment_count != 0) {
    return "orphaned_attachments";
  }
  return "clean";
}

std::filesystem::path make_temp_export_path(std::string_view filename) {
  return make_temp_vault("chem_kernel_export_") / std::string(filename);
}

AttachmentAnomalySnapshot read_attachment_anomaly_snapshot(
    const std::filesystem::path& db_path) {
  sqlite3* db = open_sqlite_readonly(db_path);
  AttachmentAnomalySnapshot snapshot{};
  snapshot.missing_attachment_count = query_single_int(
      db,
      "SELECT COUNT(*) "
      "FROM ("
      "  SELECT DISTINCT note_attachment_refs.attachment_rel_path "
      "  FROM note_attachment_refs "
      "  JOIN notes ON notes.note_id = note_attachment_refs.note_id "
      "  JOIN attachments ON attachments.rel_path = note_attachment_refs.attachment_rel_path "
      "  WHERE notes.is_deleted = 0 AND attachments.is_missing = 1"
      ");");
  snapshot.orphaned_attachment_count = query_single_int(
      db,
      "SELECT COUNT(*) "
      "FROM attachments "
      "LEFT JOIN ("
      "  SELECT DISTINCT note_attachment_refs.attachment_rel_path "
      "  FROM note_attachment_refs "
      "  JOIN notes ON notes.note_id = note_attachment_refs.note_id "
      "  WHERE notes.is_deleted = 0"
      ") AS live_refs "
      "  ON live_refs.attachment_rel_path = attachments.rel_path "
      "WHERE live_refs.attachment_rel_path IS NULL;");
  sqlite3_close(db);
  snapshot.summary = summarize_attachment_anomalies(
      snapshot.missing_attachment_count,
      snapshot.orphaned_attachment_count);
  return snapshot;
}

void require_attachment_lookup_state(
    kernel_handle* handle,
    const char* attachment_rel_path,
    const kernel_attachment_presence expected_presence,
    const std::uint64_t expected_ref_count,
    const kernel_attachment_kind expected_kind,
    const bool expect_nonzero_metadata,
    std::string_view context) {
  kernel_attachment_record attachment{};
  const kernel_status status = kernel_get_attachment(handle, attachment_rel_path, &attachment);
  require_true(
      status.code == KERNEL_OK,
      std::string(context) + ": formal attachment lookup should succeed, got status " +
          std::to_string(status.code));
  require_true(
      std::string(attachment.rel_path) == attachment_rel_path,
      std::string(context) + ": formal attachment lookup should preserve rel_path");
  require_true(
      attachment.presence == expected_presence,
      std::string(context) + ": formal attachment lookup should preserve presence");
  require_true(
      attachment.ref_count == expected_ref_count,
      std::string(context) + ": formal attachment lookup should preserve live ref_count");
  require_true(
      attachment.kind == expected_kind,
      std::string(context) + ": formal attachment lookup should preserve attachment kind");
  if (expect_nonzero_metadata) {
    require_true(
        attachment.file_size > 0,
        std::string(context) + ": formal attachment lookup should preserve non-zero file_size");
    require_true(
        attachment.mtime_ns > 0,
        std::string(context) + ": formal attachment lookup should preserve non-zero mtime_ns");
  }
  kernel_free_attachment_record(&attachment);
}

void require_single_note_attachment_ref_state(
    kernel_handle* handle,
    const char* note_rel_path,
    const char* attachment_rel_path,
    const kernel_attachment_presence expected_presence,
    const std::uint64_t expected_ref_count,
    const kernel_attachment_kind expected_kind,
    const bool expect_nonzero_metadata,
    std::string_view context) {
  kernel_attachment_list refs{};
  const kernel_status status =
      kernel_query_note_attachment_refs(handle, note_rel_path, static_cast<size_t>(-1), &refs);
  require_true(
      status.code == KERNEL_OK,
      std::string(context) + ": formal note attachment refs query should succeed, got status " +
          std::to_string(status.code));
  require_true(
      refs.count == 1,
      std::string(context) + ": formal note attachment refs should expose exactly one live attachment");
  require_true(
      std::string(refs.attachments[0].rel_path) == attachment_rel_path,
      std::string(context) + ": formal note attachment refs should expose the expected rel_path");
  require_true(
      refs.attachments[0].presence == expected_presence,
      std::string(context) + ": formal note attachment refs should preserve presence");
  require_true(
      refs.attachments[0].ref_count == expected_ref_count,
      std::string(context) + ": formal note attachment refs should preserve live ref_count");
  require_true(
      refs.attachments[0].kind == expected_kind,
      std::string(context) + ": formal note attachment refs should preserve attachment kind");
  if (expect_nonzero_metadata) {
    require_true(
        refs.attachments[0].file_size > 0,
        std::string(context) + ": formal note attachment refs should preserve non-zero file_size");
    require_true(
        refs.attachments[0].mtime_ns > 0,
        std::string(context) + ": formal note attachment refs should preserve non-zero mtime_ns");
  }
  kernel_free_attachment_list(&refs);
}

void require_note_attachment_refs_not_found(
    kernel_handle* handle,
    const char* note_rel_path,
    std::string_view context) {
  kernel_attachment_list refs{};
  const kernel_status status =
      kernel_query_note_attachment_refs(handle, note_rel_path, static_cast<size_t>(-1), &refs);
  require_true(
      status.code == KERNEL_ERROR_NOT_FOUND,
      std::string(context) + ": formal note attachment refs query should return NOT_FOUND, got status " +
          std::to_string(status.code));
  require_true(
      refs.attachments == nullptr && refs.count == 0,
      std::string(context) + ": formal note attachment refs query should clear stale output on NOT_FOUND");
}

void require_attachment_lookup_not_found(
    kernel_handle* handle,
    const char* attachment_rel_path,
    std::string_view context) {
  kernel_attachment_record attachment{};
  const kernel_status status = kernel_get_attachment(handle, attachment_rel_path, &attachment);
  require_true(
      status.code == KERNEL_ERROR_NOT_FOUND,
      std::string(context) + ": formal attachment lookup should return NOT_FOUND, got status " +
          std::to_string(status.code));
  require_true(
      attachment.rel_path == nullptr && attachment.basename == nullptr &&
          attachment.extension == nullptr && attachment.ref_count == 0,
      std::string(context) + ": formal attachment lookup should clear stale output on NOT_FOUND");
}

void require_attachment_referrers_not_found(
    kernel_handle* handle,
    const char* attachment_rel_path,
    std::string_view context) {
  kernel_attachment_referrers referrers{};
  const kernel_status status =
      kernel_query_attachment_referrers(handle, attachment_rel_path, static_cast<size_t>(-1), &referrers);
  require_true(
      status.code == KERNEL_ERROR_NOT_FOUND,
      std::string(context) + ": formal attachment referrers query should return NOT_FOUND, got status " +
          std::to_string(status.code));
  require_true(
      referrers.referrers == nullptr && referrers.count == 0,
      std::string(context) + ": formal attachment referrers query should clear stale output on NOT_FOUND");
}

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
  require_true(query_single_int(db, "PRAGMA user_version;") == 4, "storage schema should be at version 4");
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
      query_single_int(db, "SELECT COUNT(*) FROM sqlite_master WHERE type='table' AND name='note_fts';") == 1,
      "note_fts table should exist");
  sqlite3_close(db);

  expect_ok(kernel_close(handle));
  std::filesystem::remove_all(vault);
  std::filesystem::remove_all(state_dir);
}

void test_write_and_read_roundtrip() {
  const auto vault = make_temp_vault();
  kernel_handle* handle = nullptr;
  expect_ok(kernel_open_vault(vault.string().c_str(), &handle));

  kernel_note_metadata written{};
  kernel_write_disposition disposition{};
  const std::string content = "# note\nhello";
  expect_ok(kernel_write_note(
      handle,
      "note.md",
      content.data(),
      content.size(),
      nullptr,
      &written,
      &disposition));
  require_true(disposition == KERNEL_WRITE_WRITTEN, "first write should be WRITTEN");
  require_true(written.content_revision[0] != '\0', "write should populate revision");

  kernel_owned_buffer buffer{};
  kernel_note_metadata read{};
  expect_ok(kernel_read_note(handle, "note.md", &buffer, &read));
  require_true(buffer.size == content.size(), "read size should match write size");
  require_true(std::string(buffer.data, buffer.size) == content, "read content should match write content");
  require_true(
      std::string(read.content_revision) == std::string(written.content_revision),
      "read revision should match write revision");

  kernel_free_buffer(&buffer);
  expect_ok(kernel_close(handle));
  std::filesystem::remove_all(vault);
}

void test_write_appends_save_begin_and_commit_to_journal() {
  const auto vault = make_temp_vault();
  const auto state_dir = state_dir_for_vault(vault);
  const auto db_path = storage_db_for_vault(vault);
  std::filesystem::remove_all(state_dir);

  kernel_handle* handle = nullptr;
  expect_ok(kernel_open_vault(vault.string().c_str(), &handle));

  kernel_note_metadata metadata{};
  kernel_write_disposition disposition{};
  const std::string content = "journal";
  expect_ok(kernel_write_note(
      handle,
      "journal.md",
      content.data(),
      content.size(),
      nullptr,
      &metadata,
      &disposition));

  const auto journal_path = state_dir / "recovery.journal";
  require_true(std::filesystem::is_regular_file(journal_path), "write should create recovery.journal");

  const auto payloads = read_journal_payloads(journal_path);
  require_true(payloads.size() == 2, "write should append exactly two journal records");
  require_true(payloads[0].find("\"phase\":\"SAVE_BEGIN\"") != std::string::npos, "first record should be SAVE_BEGIN");
  require_true(payloads[0].find("\"rel_path\":\"journal.md\"") != std::string::npos, "first record should target the note");
  require_true(payloads[1].find("\"phase\":\"SAVE_COMMIT\"") != std::string::npos, "second record should be SAVE_COMMIT");
  require_true(payloads[1].find("\"rel_path\":\"journal.md\"") != std::string::npos, "second record should target the note");

  sqlite3* db = open_sqlite_readonly(db_path);
  require_true(
      query_single_int(db, "SELECT COUNT(*) FROM notes WHERE rel_path='journal.md' AND is_deleted=0;") == 1,
      "write should persist a notes row");
  require_true(
      query_single_text(db, "SELECT content_revision FROM notes WHERE rel_path='journal.md';") ==
          std::string(metadata.content_revision),
      "notes row should persist content revision");
  require_true(
      query_single_text(db, "SELECT title FROM notes WHERE rel_path='journal.md';") == "journal",
      "notes row should persist parser-derived title fallback");
  require_true(
      query_single_int(db, "SELECT COUNT(*) FROM journal_state WHERE rel_path='journal.md';") == 2,
      "journal_state should mirror begin and commit");
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
      query_single_int(db, "SELECT COUNT(*) FROM note_tags WHERE note_id=(SELECT note_id FROM notes WHERE rel_path='recovered.md');") == 1,
      "recovered file should persist parser-derived tags");
  require_true(
      query_single_int(db, "SELECT COUNT(*) FROM note_links WHERE note_id=(SELECT note_id FROM notes WHERE rel_path='recovered.md');") == 1,
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

  expect_empty_journal_if_present(journal_path, "truncated-tail recovery should compact journal to empty valid prefix");

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
          "SELECT tag FROM note_tags WHERE note_id=(SELECT note_id FROM notes WHERE rel_path='reopen-repair.md') LIMIT 1;") == "repairtag",
      "reopen catch-up should replace stale tags with the disk-backed tag set");
  require_true(
      query_single_text(
          db,
          "SELECT target FROM note_links WHERE note_id=(SELECT note_id FROM notes WHERE rel_path='reopen-repair.md') LIMIT 1;") == "RepairLink",
      "reopen catch-up should replace stale links with the disk-backed link set");
  sqlite3_close(db);

  expect_ok(kernel_close(handle));
  std::filesystem::remove_all(vault);
  std::filesystem::remove_all(state_dir_for_vault(vault));
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
  require_true(query_single_int(db, "PRAGMA user_version;") == 4, "schema reopen should preserve user_version=4");
  require_true(
      query_single_text(db, "SELECT value FROM schema_meta WHERE key='schema_version';") == "4",
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
  sqlite3_close(db);

  std::filesystem::remove_all(vault);
  std::filesystem::remove_all(state_dir_for_vault(vault));
}

void test_same_content_write_is_noop() {
  const auto vault = make_temp_vault();
  kernel_handle* handle = nullptr;
  expect_ok(kernel_open_vault(vault.string().c_str(), &handle));

  const std::string content = "same";
  kernel_note_metadata first{};
  kernel_write_disposition first_disposition{};
  expect_ok(kernel_write_note(
      handle,
      "same.md",
      content.data(),
      content.size(),
      nullptr,
      &first,
      &first_disposition));
  require_true(first_disposition == KERNEL_WRITE_WRITTEN, "initial same-content write should be WRITTEN");

  kernel_note_metadata second{};
  kernel_write_disposition second_disposition{};
  expect_ok(kernel_write_note(
      handle,
      "same.md",
      content.data(),
      content.size(),
      first.content_revision,
      &second,
      &second_disposition));
  require_true(second_disposition == KERNEL_WRITE_NO_OP, "same-content rewrite should be NO_OP");
  require_true(
      std::string(first.content_revision) == std::string(second.content_revision),
      "same-content rewrite should preserve revision");

  expect_ok(kernel_close(handle));
  std::filesystem::remove_all(vault);
}

void test_external_edit_causes_conflict() {
  const auto vault = make_temp_vault();
  kernel_handle* handle = nullptr;
  expect_ok(kernel_open_vault(vault.string().c_str(), &handle));

  const std::string original = "v1";
  kernel_note_metadata first{};
  kernel_write_disposition disposition{};
  expect_ok(kernel_write_note(
      handle,
      "conflict.md",
      original.data(),
      original.size(),
      nullptr,
      &first,
      &disposition));

  {
    std::ofstream output(vault / "conflict.md", std::ios::binary | std::ios::trunc);
    output << "v2";
  }

  kernel_note_metadata ignored{};
  kernel_write_disposition ignored_disposition{};
  const kernel_status status = kernel_write_note(
      handle,
      "conflict.md",
      "v3",
      2,
      first.content_revision,
      &ignored,
      &ignored_disposition);
  require_true(status.code == KERNEL_ERROR_CONFLICT, "stale revision should conflict");

  expect_ok(kernel_close(handle));
  std::filesystem::remove_all(vault);
}

void test_empty_content_note_is_allowed() {
  const auto vault = make_temp_vault();
  kernel_handle* handle = nullptr;
  expect_ok(kernel_open_vault(vault.string().c_str(), &handle));

  kernel_note_metadata metadata{};
  kernel_write_disposition disposition{};
  expect_ok(kernel_write_note(handle, "empty.md", "", 0, nullptr, &metadata, &disposition));
  require_true(disposition == KERNEL_WRITE_WRITTEN, "empty note write should be WRITTEN");

  kernel_owned_buffer buffer{};
  kernel_note_metadata read{};
  expect_ok(kernel_read_note(handle, "empty.md", &buffer, &read));
  require_true(buffer.size == 0, "empty note should read back as empty");
  require_true(
      std::string(read.content_revision) == std::string(metadata.content_revision),
      "empty note revision should roundtrip");

  kernel_free_buffer(&buffer);
  expect_ok(kernel_close(handle));
  std::filesystem::remove_all(vault);
}

void test_write_requires_output_pointers() {
  const auto vault = make_temp_vault();
  kernel_handle* handle = nullptr;
  expect_ok(kernel_open_vault(vault.string().c_str(), &handle));

  kernel_note_metadata metadata{};
  kernel_write_disposition disposition{};
  require_true(
      kernel_write_note(handle, "x.md", "", 0, nullptr, nullptr, &disposition).code == KERNEL_ERROR_INVALID_ARGUMENT,
      "write should require out_metadata");
  require_true(
      kernel_write_note(handle, "x.md", "", 0, nullptr, &metadata, nullptr).code == KERNEL_ERROR_INVALID_ARGUMENT,
      "write should require out_disposition");

  expect_ok(kernel_close(handle));
  std::filesystem::remove_all(vault);
}

void test_write_persists_parser_derived_tags_and_wikilinks() {
  const auto vault = make_temp_vault();
  const auto db_path = storage_db_for_vault(vault);
  kernel_handle* handle = nullptr;
  expect_ok(kernel_open_vault(vault.string().c_str(), &handle));

  const std::string content =
      "# Parser Title\n"
      "See [[Alpha]] and [[Beta|Shown]].\n"
      "Tags: #chem #org\n";
  kernel_note_metadata metadata{};
  kernel_write_disposition disposition{};
  expect_ok(kernel_write_note(
      handle,
      "derived.md",
      content.data(),
      content.size(),
      nullptr,
      &metadata,
      &disposition));

  sqlite3* db = open_sqlite_readonly(db_path);
  require_true(
      query_single_text(db, "SELECT title FROM notes WHERE rel_path='derived.md';") == "Parser Title",
      "write should persist parser-derived title");
  require_true(
      query_single_int(db, "SELECT COUNT(*) FROM note_tags WHERE note_id=(SELECT note_id FROM notes WHERE rel_path='derived.md');") == 2,
      "write should persist two parser-derived tags");
  require_true(
      query_single_int(db, "SELECT COUNT(*) FROM note_links WHERE note_id=(SELECT note_id FROM notes WHERE rel_path='derived.md');") == 2,
      "write should persist two parser-derived wikilinks");
  require_true(
      query_single_text(
          db,
          "SELECT tag FROM note_tags WHERE note_id=(SELECT note_id FROM notes WHERE rel_path='derived.md') ORDER BY rowid LIMIT 1;") == "chem",
      "tags should preserve parser order in storage");
  require_true(
      query_single_text(
          db,
          "SELECT target FROM note_links WHERE note_id=(SELECT note_id FROM notes WHERE rel_path='derived.md') ORDER BY rowid LIMIT 1;") == "Alpha",
      "wikilinks should preserve parser order in storage");
  sqlite3_close(db);

  expect_ok(kernel_close(handle));
  std::filesystem::remove_all(vault);
  std::filesystem::remove_all(state_dir_for_vault(vault));
}

void test_rewrite_replaces_old_parser_derived_rows() {
  const auto vault = make_temp_vault();
  const auto db_path = storage_db_for_vault(vault);
  kernel_handle* handle = nullptr;
  expect_ok(kernel_open_vault(vault.string().c_str(), &handle));

  const std::string original =
      "# Original Title\n"
      "See [[Alpha]] and [[Beta]].\n"
      "Tags: #chem #org\n";
  kernel_note_metadata first{};
  kernel_write_disposition disposition{};
  expect_ok(kernel_write_note(
      handle,
      "rewrite.md",
      original.data(),
      original.size(),
      nullptr,
      &first,
      &disposition));

  const std::string rewritten =
      "No heading now.\n"
      "See [[Gamma]].\n"
      "Tags: #newtag\n";
  kernel_note_metadata second{};
  expect_ok(kernel_write_note(
      handle,
      "rewrite.md",
      rewritten.data(),
      rewritten.size(),
      first.content_revision,
      &second,
      &disposition));

  sqlite3* db = open_sqlite_readonly(db_path);
  require_true(
      query_single_text(db, "SELECT title FROM notes WHERE rel_path='rewrite.md';") == "rewrite",
      "rewrite should replace parser title with filename fallback when heading disappears");
  require_true(
      query_single_int(db, "SELECT COUNT(*) FROM note_tags WHERE note_id=(SELECT note_id FROM notes WHERE rel_path='rewrite.md');") == 1,
      "rewrite should clear old tags before inserting new ones");
  require_true(
      query_single_text(
          db,
          "SELECT tag FROM note_tags WHERE note_id=(SELECT note_id FROM notes WHERE rel_path='rewrite.md') LIMIT 1;") == "newtag",
      "rewrite should persist only the new tag set");
  require_true(
      query_single_int(db, "SELECT COUNT(*) FROM note_links WHERE note_id=(SELECT note_id FROM notes WHERE rel_path='rewrite.md');") == 1,
      "rewrite should clear old wikilinks before inserting new ones");
  require_true(
      query_single_text(
          db,
          "SELECT target FROM note_links WHERE note_id=(SELECT note_id FROM notes WHERE rel_path='rewrite.md') LIMIT 1;") == "Gamma",
      "rewrite should persist only the new wikilink set");
  sqlite3_close(db);

  expect_ok(kernel_close(handle));
  std::filesystem::remove_all(vault);
  std::filesystem::remove_all(state_dir_for_vault(vault));
}

void test_write_persists_attachment_metadata_and_refs() {
  const auto vault = make_temp_vault();
  const auto db_path = storage_db_for_vault(vault);
  std::filesystem::create_directories(vault / "assets");
  std::filesystem::create_directories(vault / "docs");
  write_file_bytes(vault / "assets" / "diagram.png", "diagram-bytes");
  write_file_bytes(vault / "docs" / "paper.pdf", "paper-bytes");

  kernel_handle* handle = nullptr;
  expect_ok(kernel_open_vault(vault.string().c_str(), &handle));

  const std::string content =
      "# Attachment Title\n"
      "![Figure](assets/diagram.png)\n"
      "[Paper](docs/paper.pdf)\n";
  kernel_note_metadata metadata{};
  kernel_write_disposition disposition{};
  expect_ok(kernel_write_note(
      handle,
      "attachments.md",
      content.data(),
      content.size(),
      nullptr,
      &metadata,
      &disposition));

  sqlite3* db = open_sqlite_readonly(db_path);
  require_true(
      query_single_int(db, "SELECT COUNT(*) FROM attachments WHERE rel_path='assets/diagram.png' AND is_missing=0;") == 1,
      "write should register present image attachment metadata");
  require_true(
      query_single_int(db, "SELECT COUNT(*) FROM attachments WHERE rel_path='docs/paper.pdf' AND is_missing=0;") == 1,
      "write should register present document attachment metadata");
  require_true(
      query_single_int(
          db,
          "SELECT COUNT(*) FROM note_attachment_refs "
          "WHERE note_id=(SELECT note_id FROM notes WHERE rel_path='attachments.md');") == 2,
      "write should persist two note attachment refs");
  require_true(
      query_single_text(
          db,
          "SELECT attachment_rel_path FROM note_attachment_refs "
          "WHERE note_id=(SELECT note_id FROM notes WHERE rel_path='attachments.md') "
          "ORDER BY rowid LIMIT 1;") == "assets/diagram.png",
      "attachment refs should preserve parser order in storage");
  sqlite3_close(db);

  expect_ok(kernel_close(handle));
  std::filesystem::remove_all(vault);
  std::filesystem::remove_all(state_dir_for_vault(vault));
}

void test_rewrite_replaces_old_attachment_refs() {
  const auto vault = make_temp_vault();
  const auto db_path = storage_db_for_vault(vault);
  std::filesystem::create_directories(vault / "assets");
  std::filesystem::create_directories(vault / "docs");
  write_file_bytes(vault / "assets" / "first.png", "first-bytes");
  write_file_bytes(vault / "docs" / "second.pdf", "second-bytes");

  kernel_handle* handle = nullptr;
  expect_ok(kernel_open_vault(vault.string().c_str(), &handle));

  const std::string original =
      "# Attachment Rewrite\n"
      "![First](assets/first.png)\n";
  kernel_note_metadata first{};
  kernel_write_disposition disposition{};
  expect_ok(kernel_write_note(
      handle,
      "attachment-rewrite.md",
      original.data(),
      original.size(),
      nullptr,
      &first,
      &disposition));

  const std::string rewritten =
      "# Attachment Rewrite\n"
      "[Second](docs/second.pdf)\n";
  kernel_note_metadata second{};
  expect_ok(kernel_write_note(
      handle,
      "attachment-rewrite.md",
      rewritten.data(),
      rewritten.size(),
      first.content_revision,
      &second,
      &disposition));

  sqlite3* db = open_sqlite_readonly(db_path);
  require_true(
      query_single_int(
          db,
          "SELECT COUNT(*) FROM note_attachment_refs "
          "WHERE note_id=(SELECT note_id FROM notes WHERE rel_path='attachment-rewrite.md');") == 1,
      "rewrite should clear old attachment refs before inserting new ones");
  require_true(
      query_single_text(
          db,
          "SELECT attachment_rel_path FROM note_attachment_refs "
          "WHERE note_id=(SELECT note_id FROM notes WHERE rel_path='attachment-rewrite.md') LIMIT 1;") == "docs/second.pdf",
      "rewrite should persist only the new attachment ref set");
  sqlite3_close(db);

  expect_ok(kernel_close(handle));
  std::filesystem::remove_all(vault);
  std::filesystem::remove_all(state_dir_for_vault(vault));
}

void test_attachment_api_lists_note_refs_in_parser_order() {
  const auto vault = make_temp_vault();
  std::filesystem::create_directories(vault / "assets");
  std::filesystem::create_directories(vault / "docs");
  write_file_bytes(vault / "assets" / "diagram.png", "diagram-bytes");
  write_file_bytes(vault / "docs" / "paper.pdf", "paper-bytes");

  kernel_handle* handle = nullptr;
  expect_ok(kernel_open_vault(vault.string().c_str(), &handle));

  const std::string content =
      "# Attachment API\n"
      "![Figure](assets/diagram.png)\n"
      "[Paper](docs/paper.pdf)\n";
  kernel_note_metadata metadata{};
  kernel_write_disposition disposition{};
  expect_ok(kernel_write_note(
      handle,
      "attachment-api.md",
      content.data(),
      content.size(),
      nullptr,
      &metadata,
      &disposition));

  kernel_attachment_refs refs{};
  expect_ok(kernel_list_note_attachments(handle, "attachment-api.md", &refs));
  require_true(refs.count == 2, "attachment API should return two attachment refs");
  require_true(std::string(refs.refs[0].rel_path) == "assets/diagram.png", "attachment API should preserve parser order");
  require_true(std::string(refs.refs[1].rel_path) == "docs/paper.pdf", "attachment API should preserve parser order");
  kernel_free_attachment_refs(&refs);

  expect_ok(kernel_close(handle));
  std::filesystem::remove_all(vault);
  std::filesystem::remove_all(state_dir_for_vault(vault));
}

void test_attachment_api_reports_missing_state_and_rejects_invalid_inputs() {
  const auto vault = make_temp_vault();
  std::filesystem::create_directories(vault / "assets");
  std::filesystem::create_directories(vault / "docs");
  write_file_bytes(vault / "assets" / "present.png", "present-bytes");

  kernel_handle* handle = nullptr;
  expect_ok(kernel_open_vault(vault.string().c_str(), &handle));

  const std::string content =
      "# Attachment Missing\n"
      "![Present](assets/present.png)\n"
      "![Missing](docs/missing.pdf)\n";
  kernel_note_metadata metadata{};
  kernel_write_disposition disposition{};
  expect_ok(kernel_write_note(
      handle,
      "attachment-missing.md",
      content.data(),
      content.size(),
      nullptr,
      &metadata,
      &disposition));

  kernel_attachment_metadata attachment{};
  expect_ok(kernel_get_attachment_metadata(handle, "assets/present.png", &attachment));
  require_true(attachment.is_missing == 0, "present attachment metadata should report not missing");
  require_true(attachment.file_size > 0, "present attachment metadata should preserve file size");

  expect_ok(kernel_get_attachment_metadata(handle, "docs/missing.pdf", &attachment));
  require_true(attachment.is_missing == 1, "missing attachment metadata should report missing");

  require_true(
      kernel_list_note_attachments(handle, "", nullptr).code == KERNEL_ERROR_INVALID_ARGUMENT,
      "attachment refs query should reject null output");
  kernel_attachment_refs refs{};
  require_true(
      kernel_list_note_attachments(handle, "", &refs).code == KERNEL_ERROR_INVALID_ARGUMENT,
      "attachment refs query should reject empty note path");
  require_true(
      kernel_get_attachment_metadata(handle, "", &attachment).code == KERNEL_ERROR_INVALID_ARGUMENT,
      "attachment metadata query should reject empty attachment path");
  require_true(
      kernel_get_attachment_metadata(handle, "..\\bad.bin", &attachment).code == KERNEL_ERROR_INVALID_ARGUMENT,
      "attachment metadata query should reject path traversal");
  require_true(
      kernel_get_attachment_metadata(handle, "assets/unknown.bin", &attachment).code == KERNEL_ERROR_NOT_FOUND,
      "attachment metadata query should distinguish unknown paths from known missing rows");

  expect_ok(kernel_close(handle));
  std::filesystem::remove_all(vault);
  std::filesystem::remove_all(state_dir_for_vault(vault));
}

void test_attachment_public_surface_lists_live_catalog_and_single_attachment() {
  const auto vault = make_temp_vault();
  std::filesystem::create_directories(vault / "assets");
  std::filesystem::create_directories(vault / "docs");
  std::filesystem::create_directories(vault / "misc");
  write_file_bytes(vault / "assets" / "present.png", "present-bytes");
  write_file_bytes(vault / "docs" / "shared.pdf", "shared-bytes");
  write_file_bytes(vault / "misc" / "unreferenced.bin", "unreferenced-bytes");

  kernel_handle* handle = nullptr;
  expect_ok(kernel_open_vault(vault.string().c_str(), &handle));

  kernel_note_metadata metadata{};
  kernel_write_disposition disposition{};
  const std::string first =
      "# First Attachment Surface\n"
      "![Present](assets/present.png)\n"
      "[Missing](docs/missing.pdf)\n";
  expect_ok(kernel_write_note(
      handle,
      "first-surface.md",
      first.data(),
      first.size(),
      nullptr,
      &metadata,
      &disposition));

  const std::string second =
      "# Second Attachment Surface\n"
      "![Present](assets/present.png)\n"
      "[Shared](docs/shared.pdf)\n";
  expect_ok(kernel_write_note(
      handle,
      "second-surface.md",
      second.data(),
      second.size(),
      nullptr,
      &metadata,
      &disposition));

  kernel_attachment_list attachments{};
  expect_ok(kernel_query_attachments(handle, static_cast<size_t>(-1), &attachments));
  require_true(attachments.count == 3, "attachment public surface should list exactly the live catalog");

  require_true(
      std::string(attachments.attachments[0].rel_path) == "assets/present.png",
      "attachment public surface should sort the live catalog by rel_path");
  require_true(
      std::string(attachments.attachments[1].rel_path) == "docs/missing.pdf",
      "attachment public surface should keep missing live attachments in rel_path order");
  require_true(
      std::string(attachments.attachments[2].rel_path) == "docs/shared.pdf",
      "attachment public surface should include every live attachment exactly once");

  require_true(
      std::string(attachments.attachments[0].basename) == "present.png",
      "attachment public surface should expose basename for list entries");
  require_true(
      std::string(attachments.attachments[0].extension) == ".png",
      "attachment public surface should expose lowercase extensions for list entries");
  require_true(
      attachments.attachments[0].kind == KERNEL_ATTACHMENT_KIND_IMAGE_LIKE,
      "attachment public surface should classify common image attachments");
  require_true(
      attachments.attachments[0].presence == KERNEL_ATTACHMENT_PRESENCE_PRESENT,
      "attachment public surface should report present attachments");
  require_true(
      attachments.attachments[0].file_size > 0,
      "attachment public surface should expose reconciled file size for present attachments");
  require_true(
      attachments.attachments[0].mtime_ns > 0,
      "attachment public surface should expose reconciled mtime for present attachments");
  require_true(
      attachments.attachments[0].ref_count == 2,
      "attachment public surface should report the live note ref count");
  require_true(
      attachments.attachments[0].flags == KERNEL_ATTACHMENT_FLAG_NONE,
      "attachment public surface should keep undefined flags cleared");

  require_true(
      attachments.attachments[1].kind == KERNEL_ATTACHMENT_KIND_PDF_LIKE,
      "attachment public surface should classify common PDF attachments");
  require_true(
      attachments.attachments[1].presence == KERNEL_ATTACHMENT_PRESENCE_MISSING,
      "attachment public surface should preserve live missing attachments");
  require_true(
      attachments.attachments[1].file_size == 0,
      "attachment public surface should allow zero file size for missing attachments never seen on disk");
  require_true(
      attachments.attachments[1].mtime_ns == 0,
      "attachment public surface should allow zero mtime for missing attachments never seen on disk");
  require_true(
      attachments.attachments[1].ref_count == 1,
      "attachment public surface should track missing attachment ref counts");

  kernel_attachment_record attachment{};
  expect_ok(kernel_get_attachment(handle, "assets/present.png", &attachment));
  require_true(
      std::string(attachment.rel_path) == "assets/present.png",
      "single attachment lookup should return the normalized rel_path");
  require_true(
      std::string(attachment.basename) == "present.png",
      "single attachment lookup should expose basename");
  require_true(
      std::string(attachment.extension) == ".png",
      "single attachment lookup should expose lowercase extension");
  require_true(
      attachment.kind == KERNEL_ATTACHMENT_KIND_IMAGE_LIKE,
      "single attachment lookup should expose attachment kind");
  require_true(
      attachment.presence == KERNEL_ATTACHMENT_PRESENCE_PRESENT,
      "single attachment lookup should preserve present state");
  require_true(
      attachment.file_size == attachments.attachments[0].file_size,
      "single attachment lookup should preserve present file size");
  require_true(
      attachment.mtime_ns == attachments.attachments[0].mtime_ns,
      "single attachment lookup should preserve present mtime");
  require_true(
      attachment.ref_count == 2,
      "single attachment lookup should preserve the live ref count");
  kernel_free_attachment_record(&attachment);

  expect_ok(kernel_get_attachment(handle, "docs/missing.pdf", &attachment));
  require_true(
      attachment.presence == KERNEL_ATTACHMENT_PRESENCE_MISSING,
      "single attachment lookup should preserve live missing state");
  require_true(
      attachment.file_size == 0,
      "single attachment lookup should preserve zero file size for never-observed missing rows");
  require_true(
      attachment.mtime_ns == 0,
      "single attachment lookup should preserve zero mtime for never-observed missing rows");
  require_true(
      attachment.kind == KERNEL_ATTACHMENT_KIND_PDF_LIKE,
      "single attachment lookup should preserve attachment kind for missing rows");
  kernel_free_attachment_record(&attachment);

  require_true(
      kernel_get_attachment(handle, "misc/unreferenced.bin", &attachment).code ==
          KERNEL_ERROR_NOT_FOUND,
      "single attachment lookup should reject disk files that are not in the live catalog");

  kernel_free_attachment_list(&attachments);
  expect_ok(kernel_close(handle));
  std::filesystem::remove_all(vault);
  std::filesystem::remove_all(state_dir_for_vault(vault));
}

void test_attachment_public_surface_metadata_contract_covers_kind_mapping_and_missing_carry_forward() {
  const auto vault = make_temp_vault();
  std::filesystem::create_directories(vault / "assets");
  std::filesystem::create_directories(vault / "chem");
  std::filesystem::create_directories(vault / "misc");
  write_file_bytes(vault / "assets" / "data.bin", "12345");
  write_file_bytes(vault / "chem" / "ligand.CDX", "chem");
  write_file_bytes(vault / "misc" / "noext", "noext");

  kernel_handle* handle = nullptr;
  expect_ok(kernel_open_vault(vault.string().c_str(), &handle));

  kernel_note_metadata metadata{};
  kernel_write_disposition disposition{};
  const std::string content =
      "# Metadata Attachment Surface\n"
      "[Generic](assets/data.bin)\n"
      "[Chem](chem/ligand.CDX)\n";
  expect_ok(kernel_write_note(
      handle,
      "metadata-attachments.md",
      content.data(),
      content.size(),
      nullptr,
      &metadata,
      &disposition));
  {
    std::lock_guard lock(handle->storage_mutex);
    require_true(
        !kernel::index::refresh_markdown_path(handle->storage, vault, "misc/noext"),
        "metadata contract test should refresh extensionless attachment metadata");
    exec_sql(
        handle->storage.connection,
        "INSERT INTO note_attachment_refs(note_id, attachment_rel_path) "
        "VALUES((SELECT note_id FROM notes WHERE rel_path='metadata-attachments.md'), 'misc/noext');");
  }

  kernel_attachment_list attachments{};
  expect_ok(kernel_query_attachments(handle, static_cast<size_t>(-1), &attachments));
  require_true(attachments.count == 3, "metadata contract test should see all three live attachments");

  require_true(
      std::string(attachments.attachments[0].rel_path) == "assets/data.bin" &&
          attachments.attachments[0].kind == KERNEL_ATTACHMENT_KIND_GENERIC_FILE &&
          std::string(attachments.attachments[0].extension) == ".bin" &&
          attachments.attachments[0].file_size == 5 &&
          attachments.attachments[0].mtime_ns > 0,
      "metadata contract should freeze generic-file kind and stable present metadata");
  require_true(
      std::string(attachments.attachments[1].rel_path) == "chem/ligand.CDX" &&
          attachments.attachments[1].kind == KERNEL_ATTACHMENT_KIND_CHEM_LIKE &&
          std::string(attachments.attachments[1].extension) == ".cdx" &&
          attachments.attachments[1].file_size == 4 &&
          attachments.attachments[1].mtime_ns > 0,
      "metadata contract should normalize extensions and classify chem-like attachments");
  require_true(
      std::string(attachments.attachments[2].rel_path) == "misc/noext" &&
          attachments.attachments[2].kind == KERNEL_ATTACHMENT_KIND_UNKNOWN &&
          std::string(attachments.attachments[2].extension).empty() &&
          attachments.attachments[2].file_size == 5 &&
          attachments.attachments[2].mtime_ns > 0,
      "metadata contract should classify extensionless attachments as unknown");

  const auto preserved_size = attachments.attachments[0].file_size;
  const auto preserved_mtime = attachments.attachments[0].mtime_ns;
  kernel_free_attachment_list(&attachments);

  std::filesystem::remove(vault / "assets" / "data.bin");
  require_eventually(
      [&]() {
        kernel_attachment_record attachment{};
        const kernel_status status = kernel_get_attachment(handle, "assets/data.bin", &attachment);
        if (status.code != KERNEL_OK) {
          return false;
        }

        const bool matches =
            attachment.presence == KERNEL_ATTACHMENT_PRESENCE_MISSING &&
            attachment.kind == KERNEL_ATTACHMENT_KIND_GENERIC_FILE &&
            std::string(attachment.extension) == ".bin" &&
            attachment.file_size == preserved_size &&
            attachment.mtime_ns == preserved_mtime;
        kernel_free_attachment_record(&attachment);
        return matches;
      },
      "metadata contract should preserve last present size and mtime after watcher delete");

  expect_ok(kernel_close(handle));
  std::filesystem::remove_all(vault);
  std::filesystem::remove_all(state_dir_for_vault(vault));
}

void test_attachment_public_surface_note_refs_and_referrers_are_stable() {
  const auto vault = make_temp_vault();
  std::filesystem::create_directories(vault / "assets");
  std::filesystem::create_directories(vault / "docs");
  write_file_bytes(vault / "assets" / "diagram.png", "diagram-bytes");
  write_file_bytes(vault / "docs" / "paper.pdf", "paper-bytes");

  kernel_handle* handle = nullptr;
  expect_ok(kernel_open_vault(vault.string().c_str(), &handle));

  kernel_note_metadata metadata{};
  kernel_write_disposition disposition{};
  const std::string alpha =
      "# Alpha Attachment Note\n"
      "[Paper](docs/paper.pdf)\n"
      "![Diagram](assets/diagram.png)\n";
  expect_ok(kernel_write_note(
      handle,
      "alpha-attachments.md",
      alpha.data(),
      alpha.size(),
      nullptr,
      &metadata,
      &disposition));

  const std::string beta =
      "# Beta Attachment Note\n"
      "[Paper](docs/paper.pdf)\n";
  expect_ok(kernel_write_note(
      handle,
      "beta-attachments.md",
      beta.data(),
      beta.size(),
      nullptr,
      &metadata,
      &disposition));
  const std::string gamma =
      "# Gamma Attachment Note\n"
      "plain body without attachment refs\n";
  expect_ok(kernel_write_note(
      handle,
      "gamma-attachments.md",
      gamma.data(),
      gamma.size(),
      nullptr,
      &metadata,
      &disposition));

  kernel_attachment_list refs{};
  expect_ok(kernel_query_note_attachment_refs(
      handle,
      "alpha-attachments.md",
      static_cast<size_t>(-1),
      &refs));
  require_true(refs.count == 2, "note attachment refs surface should return both live attachments");
  require_true(
      std::string(refs.attachments[0].rel_path) == "docs/paper.pdf",
      "note attachment refs surface should preserve parser order");
  require_true(
      std::string(refs.attachments[1].rel_path) == "assets/diagram.png",
      "note attachment refs surface should preserve parser order across multiple refs");
  require_true(
      refs.attachments[0].ref_count == 2,
      "note attachment refs surface should expose global live ref counts");
  require_true(
      refs.attachments[1].ref_count == 1,
      "note attachment refs surface should expose per-attachment live ref counts");
  kernel_free_attachment_list(&refs);

  expect_ok(kernel_query_note_attachment_refs(
      handle,
      "gamma-attachments.md",
      static_cast<size_t>(-1),
      &refs));
  require_true(
      refs.count == 0,
      "note attachment refs surface should succeed with an empty result for live notes without attachment refs");
  kernel_free_attachment_list(&refs);

  kernel_attachment_referrers referrers{};
  expect_ok(kernel_query_attachment_referrers(
      handle,
      "docs/paper.pdf",
      static_cast<size_t>(-1),
      &referrers));
  require_true(
      referrers.count == 2,
      "attachment referrers surface should report every live note that references the attachment");
  require_true(
      std::string(referrers.referrers[0].note_rel_path) == "alpha-attachments.md",
      "attachment referrers surface should sort by note rel_path");
  require_true(
      std::string(referrers.referrers[1].note_rel_path) == "beta-attachments.md",
      "attachment referrers surface should keep note rel_path ordering stable");
  require_true(
      std::string(referrers.referrers[0].note_title) == "Alpha Attachment Note",
      "attachment referrers surface should expose note titles");
  require_true(
      std::string(referrers.referrers[1].note_title) == "Beta Attachment Note",
      "attachment referrers surface should expose note titles");
  kernel_free_attachment_referrers(&referrers);

  require_true(
      kernel_query_attachments(handle, 0, nullptr).code == KERNEL_ERROR_INVALID_ARGUMENT,
      "attachment catalog surface should reject null output and zero limit");
  require_true(
      kernel_query_note_attachment_refs(handle, "", static_cast<size_t>(-1), &refs).code ==
          KERNEL_ERROR_INVALID_ARGUMENT,
      "note attachment refs surface should reject empty note paths");
  require_true(
      kernel_query_attachment_referrers(handle, "..\\paper.pdf", static_cast<size_t>(-1), &referrers)
              .code == KERNEL_ERROR_INVALID_ARGUMENT,
      "attachment referrers surface should reject path traversal");

  expect_ok(kernel_close(handle));
  std::filesystem::remove_all(vault);
  std::filesystem::remove_all(state_dir_for_vault(vault));
}

void test_attachment_public_surface_excludes_orphaned_paths_and_matches_search() {
  const auto vault = make_temp_vault();
  const auto db_path = storage_db_for_vault(vault);
  std::filesystem::create_directories(vault / "assets");
  std::filesystem::create_directories(vault / "docs");
  write_file_bytes(vault / "assets" / "stale.png", "stale-bytes");
  write_file_bytes(vault / "docs" / "live.pdf", "live-bytes");

  kernel_handle* handle = nullptr;
  expect_ok(kernel_open_vault(vault.string().c_str(), &handle));

  kernel_note_metadata metadata{};
  kernel_write_disposition disposition{};
  const std::string original =
      "# Orphan Attachment Note\n"
      "![Stale](assets/stale.png)\n";
  expect_ok(kernel_write_note(
      handle,
      "orphan-attachments.md",
      original.data(),
      original.size(),
      nullptr,
      &metadata,
      &disposition));

  const std::string rewritten =
      "# Orphan Attachment Note\n"
      "[Live](docs/live.pdf)\n";
  expect_ok(kernel_write_note(
      handle,
      "orphan-attachments.md",
      rewritten.data(),
      rewritten.size(),
      metadata.content_revision,
      &metadata,
      &disposition));

  sqlite3* db = open_sqlite_readonly(db_path);
  require_true(
      query_single_int(db, "SELECT COUNT(*) FROM attachments WHERE rel_path='assets/stale.png';") == 1,
      "rewrite should leave the stale attachment metadata row behind for orphan filtering coverage");
  require_true(
      query_single_int(
          db,
          "SELECT COUNT(*) FROM note_attachment_refs "
          "WHERE attachment_rel_path='assets/stale.png';") == 0,
      "rewrite should clear stale note attachment refs");
  sqlite3_close(db);

  kernel_attachment_list attachments{};
  expect_ok(kernel_query_attachments(handle, static_cast<size_t>(-1), &attachments));
  require_true(attachments.count == 1, "attachment catalog surface should exclude orphaned paths");
  require_true(
      std::string(attachments.attachments[0].rel_path) == "docs/live.pdf",
      "attachment catalog surface should only keep live paths after rewrite");
  kernel_free_attachment_list(&attachments);

  kernel_attachment_record attachment{};
  require_true(
      kernel_get_attachment(handle, "assets/stale.png", &attachment).code == KERNEL_ERROR_NOT_FOUND,
      "single attachment lookup should exclude orphaned metadata rows");
  kernel_attachment_referrers referrers{};
  require_true(
      kernel_query_attachment_referrers(
          handle,
          "assets/stale.png",
          static_cast<size_t>(-1),
          &referrers)
              .code == KERNEL_ERROR_NOT_FOUND,
      "attachment referrers surface should exclude orphaned metadata rows");

  kernel_search_query request{};
  request.query = "stale";
  request.limit = 8;
  request.kind = KERNEL_SEARCH_KIND_ATTACHMENT;
  request.sort_mode = KERNEL_SEARCH_SORT_REL_PATH_ASC;
  kernel_search_page page{};
  expect_ok(kernel_query_search(handle, &request, &page));
  require_true(page.count == 0 && page.total_hits == 0, "attachment path search should exclude orphaned paths");
  kernel_free_search_page(&page);

  request.query = "live";
  expect_ok(kernel_query_search(handle, &request, &page));
  require_true(page.count == 1 && page.total_hits == 1, "attachment path search should still expose the live attachment path");
  require_true(
      std::string(page.hits[0].rel_path) == "docs/live.pdf",
      "attachment path search should agree with the public attachment surface");
  kernel_free_search_page(&page);

  expect_ok(kernel_close(handle));
  std::filesystem::remove_all(vault);
  std::filesystem::remove_all(state_dir_for_vault(vault));
}

void test_attachment_api_rewrite_recovery_and_rebuild_follow_live_state() {
  const auto vault = make_temp_vault();
  std::filesystem::create_directories(vault / "assets");
  std::filesystem::create_directories(vault / "docs");
  write_file_bytes(vault / "assets" / "first.png", "first-bytes");
  write_file_bytes(vault / "docs" / "recovered.pdf", "recovered-bytes");

  kernel_handle* handle = nullptr;
  expect_ok(kernel_open_vault(vault.string().c_str(), &handle));

  kernel_note_metadata metadata{};
  kernel_write_disposition disposition{};
  const std::string original =
      "# Attachment Surface\n"
      "![First](assets/first.png)\n";
  expect_ok(kernel_write_note(
      handle,
      "attachment-surface.md",
      original.data(),
      original.size(),
      nullptr,
      &metadata,
      &disposition));

  const std::string rewritten =
      "# Attachment Surface\n"
      "[Recovered](docs/recovered.pdf)\n";
  expect_ok(kernel_write_note(
      handle,
      "attachment-surface.md",
      rewritten.data(),
      rewritten.size(),
      metadata.content_revision,
      &metadata,
      &disposition));

  require_single_note_attachment_ref_state(
      handle,
      "attachment-surface.md",
      "docs/recovered.pdf",
      KERNEL_ATTACHMENT_PRESENCE_PRESENT,
      1,
      KERNEL_ATTACHMENT_KIND_PDF_LIKE,
      true,
      "rewrite should replace old attachment refs in the formal public surface");

  {
    std::lock_guard lock(handle->storage_mutex);
    exec_sql(handle->storage.connection, "DELETE FROM note_attachment_refs WHERE note_id=(SELECT note_id FROM notes WHERE rel_path='attachment-surface.md');");
    exec_sql(handle->storage.connection, "INSERT INTO note_attachment_refs(note_id, attachment_rel_path) VALUES((SELECT note_id FROM notes WHERE rel_path='attachment-surface.md'), 'assets/stale.bin');");
    exec_sql(handle->storage.connection, "UPDATE attachments SET is_missing=0 WHERE rel_path='docs/recovered.pdf';");
  }

  expect_ok(kernel_rebuild_index(handle));

  require_single_note_attachment_ref_state(
      handle,
      "attachment-surface.md",
      "docs/recovered.pdf",
      KERNEL_ATTACHMENT_PRESENCE_PRESENT,
      1,
      KERNEL_ATTACHMENT_KIND_PDF_LIKE,
      true,
      "rebuild should restore the formal live attachment ref set");
  require_attachment_lookup_state(
      handle,
      "docs/recovered.pdf",
      KERNEL_ATTACHMENT_PRESENCE_PRESENT,
      1,
      KERNEL_ATTACHMENT_KIND_PDF_LIKE,
      true,
      "rebuild should preserve formal attachment metadata");

  expect_ok(kernel_close(handle));

  prepare_state_dir_for_vault(vault);
  const auto journal_path = journal_path_for_vault(vault);
  const auto temp_path = vault / "attachment-surface.md.recovery.tmp";
  const std::string recovered =
      "# Attachment Surface\n"
      "![Recovered Missing](docs/recovered-missing.pdf)\n";
  write_file_bytes(vault / "attachment-surface.md", recovered);
  write_file_bytes(temp_path, "stale temp");
  require_true(
      !kernel::recovery::append_save_begin(
          journal_path,
          "attachment-surface-recovery-op",
          "attachment-surface.md",
          temp_path),
      "attachment surface recovery journal append should succeed");

  handle = nullptr;
  expect_ok(kernel_open_vault(vault.string().c_str(), &handle));
  require_index_ready(handle, "attachment public API recovery test should settle to READY");

  require_single_note_attachment_ref_state(
      handle,
      "attachment-surface.md",
      "docs/recovered-missing.pdf",
      KERNEL_ATTACHMENT_PRESENCE_MISSING,
      1,
      KERNEL_ATTACHMENT_KIND_PDF_LIKE,
      false,
      "startup recovery should restore the recovered attachment ref in the formal public surface");
  require_attachment_lookup_state(
      handle,
      "docs/recovered-missing.pdf",
      KERNEL_ATTACHMENT_PRESENCE_MISSING,
      1,
      KERNEL_ATTACHMENT_KIND_PDF_LIKE,
      false,
      "startup recovery should expose recovered missing attachment metadata through the formal public surface");

  expect_ok(kernel_close(handle));
  std::filesystem::remove_all(vault);
  std::filesystem::remove_all(state_dir_for_vault(vault));
}

void test_attachment_api_observes_attachment_rename_reconciliation() {
  const auto vault = make_temp_vault();
  std::filesystem::create_directories(vault / "assets");
  write_file_bytes(vault / "assets" / "old.bin", "old-attachment");

  kernel_handle* handle = nullptr;
  expect_ok(kernel_open_vault(vault.string().c_str(), &handle));
  kernel_note_metadata metadata{};
  kernel_write_disposition disposition{};
  const std::string content =
      "# Attachment Rename\n"
      "[Old](assets/old.bin)\n";
  expect_ok(kernel_write_note(
      handle,
      "attachment-rename.md",
      content.data(),
      content.size(),
      nullptr,
      &metadata,
      &disposition));
  expect_ok(kernel_close(handle));

  auto db = kernel::storage::Database{};
  require_true(!kernel::storage::open_or_create(storage_db_for_vault(vault), db), "attachment rename API test should open storage db");
  require_true(!kernel::storage::ensure_schema_v1(db), "attachment rename API test should ensure schema");

  std::filesystem::rename(vault / "assets" / "old.bin", vault / "assets" / "renamed.bin");
  const std::vector<kernel::watcher::CoalescedAction> actions = {
      {kernel::watcher::CoalescedActionKind::RenamePath, "assets/old.bin", "assets/renamed.bin"}};
  require_true(!kernel::watcher::apply_actions(db, vault, actions), "attachment rename apply should succeed");
  kernel::storage::close(db);

  handle = nullptr;
  expect_ok(kernel_open_vault(vault.string().c_str(), &handle));
  require_index_ready(handle, "attachment rename API test should settle to READY");

  kernel_attachment_metadata attachment{};
  expect_ok(kernel_get_attachment_metadata(handle, "assets/old.bin", &attachment));
  require_true(attachment.is_missing == 1, "attachment rename should expose the old path as missing");
  expect_ok(kernel_get_attachment_metadata(handle, "assets/renamed.bin", &attachment));
  require_true(attachment.is_missing == 0, "attachment rename should expose the new path as present");

  kernel_attachment_list attachments{};
  expect_ok(kernel_query_attachments(handle, static_cast<size_t>(-1), &attachments));
  require_true(
      attachments.count == 1,
      "attachment rename should keep only the still-referenced path in the formal live catalog");
  require_true(
      std::string(attachments.attachments[0].rel_path) == "assets/old.bin" &&
          attachments.attachments[0].presence == KERNEL_ATTACHMENT_PRESENCE_MISSING,
      "attachment rename should keep the old live attachment path visible as missing in the formal catalog");
  kernel_free_attachment_list(&attachments);

  require_attachment_lookup_state(
      handle,
      "assets/old.bin",
      KERNEL_ATTACHMENT_PRESENCE_MISSING,
      1,
      KERNEL_ATTACHMENT_KIND_GENERIC_FILE,
      true,
      "attachment rename should keep the old ref path visible in the formal single-attachment surface");
  require_single_note_attachment_ref_state(
      handle,
      "attachment-rename.md",
      "assets/old.bin",
      KERNEL_ATTACHMENT_PRESENCE_MISSING,
      1,
      KERNEL_ATTACHMENT_KIND_GENERIC_FILE,
      true,
      "attachment rename should keep note attachment refs pinned to the old live path until note rewrite");
  require_attachment_lookup_not_found(
      handle,
      "assets/renamed.bin",
      "attachment rename should exclude the unreferenced renamed path from the formal single-attachment surface");

  kernel_attachment_referrers referrers{};
  expect_ok(kernel_query_attachment_referrers(
      handle,
      "assets/old.bin",
      static_cast<size_t>(-1),
      &referrers));
  require_true(
      referrers.count == 1,
      "attachment rename should keep the old live path's formal referrers visible");
  require_true(
      std::string(referrers.referrers[0].note_rel_path) == "attachment-rename.md",
      "attachment rename should keep referrers pinned to the old live path");
  kernel_free_attachment_referrers(&referrers);
  require_attachment_referrers_not_found(
      handle,
      "assets/renamed.bin",
      "attachment rename should exclude the unreferenced renamed path from the formal referrers surface");

  kernel_search_query request{};
  request.query = "old";
  request.limit = 8;
  request.kind = KERNEL_SEARCH_KIND_ATTACHMENT;
  request.sort_mode = KERNEL_SEARCH_SORT_REL_PATH_ASC;
  kernel_search_page page{};
  expect_ok(kernel_query_search(handle, &request, &page));
  require_true(
      page.count == 1 && page.total_hits == 1,
      "attachment rename should keep the old live path searchable through the attachment path surface");
  require_true(
      std::string(page.hits[0].rel_path) == "assets/old.bin" &&
          page.hits[0].result_flags == KERNEL_SEARCH_RESULT_FLAG_ATTACHMENT_MISSING,
      "attachment rename search should expose the old live path as missing");
  kernel_free_search_page(&page);

  request.query = "renamed";
  expect_ok(kernel_query_search(handle, &request, &page));
  require_true(
      page.count == 0 && page.total_hits == 0,
      "attachment rename should exclude the unreferenced renamed path from attachment path search");
  kernel_free_search_page(&page);

  expect_ok(kernel_close(handle));
  std::filesystem::remove_all(vault);
  std::filesystem::remove_all(state_dir_for_vault(vault));
}

void test_attachment_full_rescan_reconciles_mixed_changes_through_formal_public_surface() {
  const auto vault = make_temp_vault();
  std::filesystem::create_directories(vault / "assets");
  std::filesystem::create_directories(vault / "docs");
  write_file_bytes(vault / "assets" / "live.png", "live-attachment");
  write_file_bytes(vault / "assets" / "remove.bin", "remove-attachment");
  write_file_bytes(vault / "assets" / "stale.png", "stale-attachment");

  kernel_handle* handle = nullptr;
  require_true(
      kernel_open_vault(vault.string().c_str(), &handle).code == KERNEL_OK,
      "attachment full-rescan test should open vault");

  kernel_note_metadata metadata{};
  kernel_write_disposition disposition{};
  const std::string before =
      "# Attachment Full Rescan Before\n"
      "![Live](assets/live.png)\n"
      "[Remove](assets/remove.bin)\n"
      "![Stale](assets/stale.png)\n";
  require_true(
      kernel_write_note(
          handle,
          "attachment-full-rescan.md",
          before.data(),
          before.size(),
          nullptr,
          &metadata,
          &disposition)
              .code == KERNEL_OK,
      "attachment full-rescan test should seed the initial note");
  require_true(
      kernel_close(handle).code == KERNEL_OK,
      "attachment full-rescan test should close the seeded vault");

  std::filesystem::remove(vault / "assets" / "stale.png");
  write_file_bytes(vault / "docs" / "fresh.pdf", "fresh-pdf");
  const std::string after =
      "# Attachment Full Rescan After\n"
      "![Live](assets/live.png)\n"
      "![Stale](assets/stale.png)\n"
      "[Fresh](docs/fresh.pdf)\n";
  write_file_bytes(vault / "attachment-full-rescan.md", after);

  auto db = kernel::storage::Database{};
  require_true(
      !kernel::storage::open_or_create(storage_db_for_vault(vault), db),
      "attachment full-rescan test should open storage db");
  require_true(
      !kernel::storage::ensure_schema_v1(db),
      "attachment full-rescan test should ensure schema");
  const std::vector<kernel::watcher::CoalescedAction> actions = {
      {kernel::watcher::CoalescedActionKind::FullRescan, ""}};
  require_true(
      !kernel::watcher::apply_actions(db, vault, actions),
      "attachment full-rescan apply should succeed");
  kernel::storage::close(db);

  handle = nullptr;
  require_true(
      kernel_open_vault(vault.string().c_str(), &handle).code == KERNEL_OK,
      "attachment full-rescan test should reopen vault after offline full rescan");
  require_index_ready(handle, "attachment full-rescan test should settle to READY");

  kernel_attachment_list refs{};
  require_true(
      kernel_query_note_attachment_refs(
          handle,
          "attachment-full-rescan.md",
          static_cast<size_t>(-1),
          &refs)
              .code == KERNEL_OK,
      "attachment full-rescan should expose refreshed note attachment refs");
  require_true(
      refs.count == 3,
      "attachment full-rescan should replace the live note attachment ref set in the formal public surface");
  require_true(
      std::string(refs.attachments[0].rel_path) == "assets/live.png" &&
          refs.attachments[0].presence == KERNEL_ATTACHMENT_PRESENCE_PRESENT &&
          refs.attachments[0].kind == KERNEL_ATTACHMENT_KIND_IMAGE_LIKE,
      "attachment full-rescan should preserve the still-live present attachment ref");
  require_true(
      std::string(refs.attachments[1].rel_path) == "assets/stale.png" &&
          refs.attachments[1].presence == KERNEL_ATTACHMENT_PRESENCE_MISSING &&
          refs.attachments[1].kind == KERNEL_ATTACHMENT_KIND_IMAGE_LIKE,
      "attachment full-rescan should keep deleted-but-still-referenced attachments visible as missing");
  require_true(
      std::string(refs.attachments[2].rel_path) == "docs/fresh.pdf" &&
          refs.attachments[2].presence == KERNEL_ATTACHMENT_PRESENCE_PRESENT &&
          refs.attachments[2].kind == KERNEL_ATTACHMENT_KIND_PDF_LIKE,
      "attachment full-rescan should add newly referenced attachments to the formal note refs surface");
  kernel_free_attachment_list(&refs);

  kernel_attachment_list attachments{};
  require_true(
      kernel_query_attachments(handle, static_cast<size_t>(-1), &attachments).code == KERNEL_OK,
      "attachment full-rescan should expose the refreshed live catalog");
  require_true(
      attachments.count == 3,
      "attachment full-rescan should expose only the refreshed live catalog through the formal list surface");
  require_true(
      std::string(attachments.attachments[0].rel_path) == "assets/live.png" &&
          attachments.attachments[0].presence == KERNEL_ATTACHMENT_PRESENCE_PRESENT,
      "attachment full-rescan catalog should keep present live paths");
  require_true(
      std::string(attachments.attachments[1].rel_path) == "assets/stale.png" &&
          attachments.attachments[1].presence == KERNEL_ATTACHMENT_PRESENCE_MISSING,
      "attachment full-rescan catalog should keep missing live paths");
  require_true(
      std::string(attachments.attachments[2].rel_path) == "docs/fresh.pdf" &&
          attachments.attachments[2].presence == KERNEL_ATTACHMENT_PRESENCE_PRESENT,
      "attachment full-rescan catalog should add new live paths");
  kernel_free_attachment_list(&attachments);

  require_attachment_lookup_state(
      handle,
      "assets/live.png",
      KERNEL_ATTACHMENT_PRESENCE_PRESENT,
      1,
      KERNEL_ATTACHMENT_KIND_IMAGE_LIKE,
      true,
      "attachment full-rescan should preserve present attachment lookup state");
  require_attachment_lookup_state(
      handle,
      "assets/stale.png",
      KERNEL_ATTACHMENT_PRESENCE_MISSING,
      1,
      KERNEL_ATTACHMENT_KIND_IMAGE_LIKE,
      true,
      "attachment full-rescan should reconcile deleted attachment lookup state to missing");
  require_attachment_lookup_state(
      handle,
      "docs/fresh.pdf",
      KERNEL_ATTACHMENT_PRESENCE_PRESENT,
      1,
      KERNEL_ATTACHMENT_KIND_PDF_LIKE,
      true,
      "attachment full-rescan should expose newly referenced attachment lookup state");
  require_attachment_lookup_not_found(
      handle,
      "assets/remove.bin",
      "attachment full-rescan should drop removed refs from the formal live catalog");
  require_attachment_referrers_not_found(
      handle,
      "assets/remove.bin",
      "attachment full-rescan should drop removed refs from the formal referrers surface");

  kernel_search_query request{};
  request.limit = 8;
  request.kind = KERNEL_SEARCH_KIND_ATTACHMENT;
  request.sort_mode = KERNEL_SEARCH_SORT_REL_PATH_ASC;

  kernel_search_page page{};
  request.query = "fresh";
  require_true(
      kernel_query_search(handle, &request, &page).code == KERNEL_OK,
      "attachment full-rescan search should query fresh attachment paths");
  require_true(
      page.count == 1 && page.total_hits == 1 &&
          std::string(page.hits[0].rel_path) == "docs/fresh.pdf" &&
          page.hits[0].result_flags == KERNEL_SEARCH_RESULT_FLAG_NONE,
      "attachment full-rescan search should expose newly referenced live attachment paths");
  kernel_free_search_page(&page);

  request.query = "stale";
  require_true(
      kernel_query_search(handle, &request, &page).code == KERNEL_OK,
      "attachment full-rescan search should query missing live attachment paths");
  require_true(
      page.count == 1 && page.total_hits == 1 &&
          std::string(page.hits[0].rel_path) == "assets/stale.png" &&
          page.hits[0].result_flags == KERNEL_SEARCH_RESULT_FLAG_ATTACHMENT_MISSING,
      "attachment full-rescan search should keep deleted-but-live attachment paths marked as missing");
  kernel_free_search_page(&page);

  request.query = "remove";
  require_true(
      kernel_query_search(handle, &request, &page).code == KERNEL_OK,
      "attachment full-rescan search should query removed attachment paths");
  require_true(
      page.count == 0 && page.total_hits == 0,
      "attachment full-rescan search should exclude paths that are no longer in the live catalog");
  kernel_free_search_page(&page);

  require_true(
      kernel_close(handle).code == KERNEL_OK,
      "attachment full-rescan test should close the reopened vault");
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
      query_single_int(db, "SELECT COUNT(*) FROM note_tags WHERE note_id=(SELECT note_id FROM notes WHERE rel_path='recover-rewrite.md');") == 1,
      "startup recovery should clear stale tags before inserting new ones");
  require_true(
      query_single_text(
          db,
          "SELECT tag FROM note_tags WHERE note_id=(SELECT note_id FROM notes WHERE rel_path='recover-rewrite.md') LIMIT 1;") == "newtag",
      "startup recovery should persist only the recovered tag set");
  require_true(
      query_single_int(db, "SELECT COUNT(*) FROM note_links WHERE note_id=(SELECT note_id FROM notes WHERE rel_path='recover-rewrite.md');") == 1,
      "startup recovery should clear stale wikilinks before inserting new ones");
  require_true(
      query_single_text(
          db,
          "SELECT target FROM note_links WHERE note_id=(SELECT note_id FROM notes WHERE rel_path='recover-rewrite.md') LIMIT 1;") == "NewLink",
      "startup recovery should persist only the recovered wikilink set");
  sqlite3_close(db);

  expect_ok(kernel_close(handle));
  std::filesystem::remove_all(vault);
  std::filesystem::remove_all(state_dir);
}

void test_startup_recovery_replaces_stale_attachment_refs_and_metadata() {
  const auto vault = make_temp_vault();
  const auto db_path = storage_db_for_vault(vault);
  const auto state_dir = state_dir_for_vault(vault);
  const auto journal_path = journal_path_for_vault(vault);
  const auto target_path = vault / "recover-attachments.md";
  const auto temp_path = target_path.parent_path() / "recover-attachments.md.tmp";

  std::filesystem::create_directories(vault / "assets");
  std::filesystem::create_directories(vault / "docs");
  write_file_bytes(vault / "assets" / "old.png", "old-attachment-bytes");
  write_file_bytes(vault / "docs" / "new.pdf", "new-attachment-bytes");

  kernel_handle* handle = nullptr;
  expect_ok(kernel_open_vault(vault.string().c_str(), &handle));

  const std::string original =
      "# Recover Attachments\n"
      "![Old](assets/old.png)\n";
  kernel_note_metadata metadata{};
  kernel_write_disposition disposition{};
  expect_ok(kernel_write_note(
      handle,
      "recover-attachments.md",
      original.data(),
      original.size(),
      nullptr,
      &metadata,
      &disposition));
  expect_ok(kernel_close(handle));

  write_file_bytes(
      target_path,
      "# Recover Attachments\n"
      "[New](docs/new.pdf)\n");
  write_file_bytes(temp_path, "stale-temp");
  require_true(
      kernel::recovery::append_save_begin(
          journal_path,
          "recover-attachments-op",
          "recover-attachments.md",
          temp_path)
          .value() == 0,
      "attachment recovery SAVE_BEGIN append should succeed");

  expect_ok(kernel_open_vault(vault.string().c_str(), &handle));

  sqlite3* db = open_sqlite_readonly(db_path);
  require_true(
      query_single_int(
          db,
          "SELECT COUNT(*) FROM note_attachment_refs "
          "WHERE note_id=(SELECT note_id FROM notes WHERE rel_path='recover-attachments.md');") == 1,
      "startup recovery should clear stale attachment refs before inserting recovered refs");
  require_true(
      query_single_text(
          db,
          "SELECT attachment_rel_path FROM note_attachment_refs "
          "WHERE note_id=(SELECT note_id FROM notes WHERE rel_path='recover-attachments.md') LIMIT 1;") == "docs/new.pdf",
      "startup recovery should persist only the recovered attachment ref set");
  require_true(
      query_single_int(db, "SELECT COUNT(*) FROM attachments WHERE rel_path='docs/new.pdf' AND is_missing=0;") == 1,
      "startup recovery should sync recovered attachment metadata from disk truth");
  sqlite3_close(db);

  require_single_note_attachment_ref_state(
      handle,
      "recover-attachments.md",
      "docs/new.pdf",
      KERNEL_ATTACHMENT_PRESENCE_PRESENT,
      1,
      KERNEL_ATTACHMENT_KIND_PDF_LIKE,
      true,
      "startup recovery should expose the recovered attachment ref through the formal public surface");
  require_attachment_lookup_state(
      handle,
      "docs/new.pdf",
      KERNEL_ATTACHMENT_PRESENCE_PRESENT,
      1,
      KERNEL_ATTACHMENT_KIND_PDF_LIKE,
      true,
      "startup recovery should expose recovered attachment metadata through the formal public surface");

  expect_ok(kernel_close(handle));
  std::filesystem::remove_all(vault);
  std::filesystem::remove_all(state_dir);
}

void test_startup_recovery_plus_reopen_catch_up_removes_deleted_note_drift() {
  const auto vault = make_temp_vault();
  const auto db_path = storage_db_for_vault(vault);
  const auto state_dir = state_dir_for_vault(vault);
  const auto journal_path = journal_path_for_vault(vault);
  const auto target_path = vault / "recover-delete.md";
  const auto temp_path = target_path.parent_path() / "recover-delete.md.tmp";

  std::filesystem::create_directories(vault / "assets");
  write_file_bytes(vault / "assets" / "delete.png", "delete-attachment-bytes");

  kernel_handle* handle = nullptr;
  expect_ok(kernel_open_vault(vault.string().c_str(), &handle));

  const std::string original =
      "# Recover Delete\n"
      "recover-delete-live-token\n"
      "Tags: #deletetag\n"
      "See [[DeleteLink]].\n"
      "![Delete](assets/delete.png)\n";
  kernel_note_metadata metadata{};
  kernel_write_disposition disposition{};
  expect_ok(kernel_write_note(
      handle,
      "recover-delete.md",
      original.data(),
      original.size(),
      nullptr,
      &metadata,
      &disposition));
  expect_ok(kernel_close(handle));

  std::filesystem::remove(target_path);
  write_file_bytes(temp_path, "stale-delete-temp");
  require_true(
      kernel::recovery::append_save_begin(
          journal_path,
          "recover-delete-op",
          "recover-delete.md",
          temp_path)
          .value() == 0,
      "deleted-note recovery SAVE_BEGIN append should succeed");

  expect_ok(kernel_open_vault(vault.string().c_str(), &handle));
  require_index_ready(handle, "deleted-note recovery test should settle to READY after reopen catch-up");

  kernel_search_results results{};
  expect_ok(kernel_search_notes(handle, "recover-delete-live-token", &results));
  require_true(results.count == 0, "reopen catch-up should remove stale search hits for a note deleted while the kernel was closed");
  kernel_free_search_results(&results);

  sqlite3* db = open_sqlite_readonly(db_path);
  require_true(
      query_single_int(db, "SELECT is_deleted FROM notes WHERE rel_path='recover-delete.md';") == 1,
      "reopen catch-up should mark the deleted note row as is_deleted=1");
  require_true(
      query_single_int(db, "SELECT COUNT(*) FROM note_tags WHERE note_id=(SELECT note_id FROM notes WHERE rel_path='recover-delete.md');") == 0,
      "reopen catch-up should clear stale tags for a deleted note");
  require_true(
      query_single_int(db, "SELECT COUNT(*) FROM note_links WHERE note_id=(SELECT note_id FROM notes WHERE rel_path='recover-delete.md');") == 0,
      "reopen catch-up should clear stale links for a deleted note");
  require_true(
      query_single_int(db, "SELECT COUNT(*) FROM note_attachment_refs WHERE note_id=(SELECT note_id FROM notes WHERE rel_path='recover-delete.md');") == 0,
      "reopen catch-up should clear stale attachment refs for a deleted note");
  sqlite3_close(db);

  require_note_attachment_refs_not_found(
      handle,
      "recover-delete.md",
      "deleted-note recovery should remove the note from the formal attachment refs surface");
  require_attachment_lookup_not_found(
      handle,
      "assets/delete.png",
      "deleted-note recovery should remove the deleted note's attachment from the formal live catalog");
  require_attachment_referrers_not_found(
      handle,
      "assets/delete.png",
      "deleted-note recovery should remove the deleted note's attachment referrers from the formal surface");

  expect_ok(kernel_close(handle));
  std::filesystem::remove_all(vault);
  std::filesystem::remove_all(state_dir);
}

void test_reopen_catch_up_repairs_attachment_missing_state_after_closed_window_delete() {
  const auto vault = make_temp_vault();
  const auto db_path = storage_db_for_vault(vault);
  std::filesystem::create_directories(vault / "assets");
  write_file_bytes(vault / "assets" / "closed-window-delete.png", "attachment-bytes");

  kernel_handle* handle = nullptr;
  expect_ok(kernel_open_vault(vault.string().c_str(), &handle));

  const std::string content =
      "# Closed Window Attachment\n"
      "![Figure](assets/closed-window-delete.png)\n";
  kernel_note_metadata metadata{};
  kernel_write_disposition disposition{};
  expect_ok(kernel_write_note(
      handle,
      "closed-window-attachment.md",
      content.data(),
      content.size(),
      nullptr,
      &metadata,
      &disposition));
  require_index_ready(handle, "attachment closed-window delete test should start from READY");
  expect_ok(kernel_close(handle));

  std::filesystem::remove(vault / "assets" / "closed-window-delete.png");

  handle = nullptr;
  expect_ok(kernel_open_vault(vault.string().c_str(), &handle));
  require_index_ready(handle, "reopen after closed-window attachment delete should settle to READY");

  sqlite3* db = open_sqlite_readonly(db_path);
  require_true(
      query_single_int(
          db,
          "SELECT COUNT(*) FROM attachments "
          "WHERE rel_path='assets/closed-window-delete.png' AND is_missing=1;") == 1,
      "reopen catch-up should mark a closed-window attachment delete as missing");
  require_true(
      query_single_int(
          db,
          "SELECT COUNT(*) FROM note_attachment_refs "
          "WHERE note_id=(SELECT note_id FROM notes WHERE rel_path='closed-window-attachment.md') "
          "AND attachment_rel_path='assets/closed-window-delete.png';") == 1,
      "reopen catch-up should preserve note attachment refs while reconciling missing attachment state");
  sqlite3_close(db);

  require_single_note_attachment_ref_state(
      handle,
      "closed-window-attachment.md",
      "assets/closed-window-delete.png",
      KERNEL_ATTACHMENT_PRESENCE_MISSING,
      1,
      KERNEL_ATTACHMENT_KIND_IMAGE_LIKE,
      true,
      "reopen catch-up should preserve the live attachment ref through the formal public surface");
  require_attachment_lookup_state(
      handle,
      "assets/closed-window-delete.png",
      KERNEL_ATTACHMENT_PRESENCE_MISSING,
      1,
      KERNEL_ATTACHMENT_KIND_IMAGE_LIKE,
      true,
      "reopen catch-up should expose missing attachment metadata through the formal public surface");

  expect_ok(kernel_close(handle));
  std::filesystem::remove_all(vault);
  std::filesystem::remove_all(state_dir_for_vault(vault));
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
      query_single_int(db, "SELECT COUNT(*) FROM note_tags WHERE note_id=(SELECT note_id FROM notes WHERE rel_path='backoff-delete.md');") == 1,
      "close during watcher backoff should leave stale note tags until reopen catch-up");
  require_true(
      query_single_int(db, "SELECT COUNT(*) FROM note_links WHERE note_id=(SELECT note_id FROM notes WHERE rel_path='backoff-delete.md');") == 1,
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
      query_single_int(db, "SELECT COUNT(*) FROM note_tags WHERE note_id=(SELECT note_id FROM notes WHERE rel_path='backoff-delete.md');") == 0,
      "reopen catch-up should clear stale tags left by a delete during watcher backoff");
  require_true(
      query_single_int(db, "SELECT COUNT(*) FROM note_links WHERE note_id=(SELECT note_id FROM notes WHERE rel_path='backoff-delete.md');") == 0,
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
          "SELECT tag FROM note_tags WHERE note_id=(SELECT note_id FROM notes WHERE rel_path='backoff-modify.md') LIMIT 1;") == "backoffold",
      "close during watcher backoff should leave stale tags until reopen catch-up");
  require_true(
      query_single_text(
          db,
          "SELECT target FROM note_links WHERE note_id=(SELECT note_id FROM notes WHERE rel_path='backoff-modify.md') LIMIT 1;") == "BackoffOldLink",
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
          "SELECT tag FROM note_tags WHERE note_id=(SELECT note_id FROM notes WHERE rel_path='backoff-modify.md') LIMIT 1;") == "backoffnew",
      "reopen catch-up should replace stale tags after watcher-backoff shutdown");
  require_true(
      query_single_text(
          db,
          "SELECT target FROM note_links WHERE note_id=(SELECT note_id FROM notes WHERE rel_path='backoff-modify.md') LIMIT 1;") == "BackoffNewLink",
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
          "SELECT tag FROM note_tags WHERE note_id=(SELECT note_id FROM notes WHERE rel_path='backoff-create.md') LIMIT 1;") == "backoffcreate",
      "reopen catch-up should persist created tags after watcher-backoff shutdown");
  require_true(
      query_single_text(
          db,
          "SELECT target FROM note_links WHERE note_id=(SELECT note_id FROM notes WHERE rel_path='backoff-create.md') LIMIT 1;") == "BackoffCreateLink",
      "reopen catch-up should persist created links after watcher-backoff shutdown");
  sqlite3_close(db);

  expect_ok(kernel_close(handle));
  std::filesystem::remove_all(vault);
  std::filesystem::remove_all(state_dir_for_vault(vault));
}

void test_close_during_watcher_fault_backoff_leaves_attachment_create_for_reopen_catch_up() {
  const auto vault = make_temp_vault();
  const auto db_path = storage_db_for_vault(vault);
  std::filesystem::create_directories(vault / "assets");

  kernel_handle* handle = nullptr;
  expect_ok(kernel_open_vault(vault.string().c_str(), &handle));

  const std::string content =
      "# Backoff Attachment Create\n"
      "![Figure](assets/backoff-created.png)\n";
  kernel_note_metadata metadata{};
  kernel_write_disposition disposition{};
  expect_ok(kernel_write_note(
      handle,
      "backoff-attachment-create.md",
      content.data(),
      content.size(),
      nullptr,
      &metadata,
      &disposition));
  require_index_ready(handle, "watcher backoff attachment-create test should start from READY");

  sqlite3* db = open_sqlite_readonly(db_path);
  require_true(
      query_single_int(
          db,
          "SELECT COUNT(*) FROM attachments "
          "WHERE rel_path='assets/backoff-created.png' AND is_missing=1;") == 1,
      "seed write should register the missing attachment before the closed-window create");
  sqlite3_close(db);

  kernel::watcher::inject_next_poll_errors(handle->watcher_session, std::errc::io_error, 20);
  require_eventually(
      [&]() {
        kernel_state_snapshot snapshot{};
        return kernel_get_state(handle, &snapshot).code == KERNEL_OK &&
               snapshot.index_state == KERNEL_INDEX_UNAVAILABLE;
      },
      "watcher should enter UNAVAILABLE backoff before closed-window attachment create");

  write_file_bytes(vault / "assets" / "backoff-created.png", "attachment-bytes");
  expect_ok(kernel_close(handle));

  db = open_sqlite_readonly(db_path);
  require_true(
      query_single_int(
          db,
          "SELECT COUNT(*) FROM attachments "
          "WHERE rel_path='assets/backoff-created.png' AND is_missing=1;") == 1,
      "close during watcher backoff should leave the stale missing attachment row for reopen catch-up to reconcile");
  sqlite3_close(db);

  handle = nullptr;
  expect_ok(kernel_open_vault(vault.string().c_str(), &handle));
  require_index_ready(handle, "reopen after watcher-backoff attachment create should settle to READY");

  db = open_sqlite_readonly(db_path);
  require_true(
      query_single_int(
          db,
          "SELECT COUNT(*) FROM attachments "
          "WHERE rel_path='assets/backoff-created.png' AND is_missing=0;") == 1,
      "reopen catch-up should reconcile a closed-window attachment create back to present state");
  require_true(
      query_single_int(
          db,
          "SELECT COUNT(*) FROM note_attachment_refs "
          "WHERE note_id=(SELECT note_id FROM notes WHERE rel_path='backoff-attachment-create.md') "
          "AND attachment_rel_path='assets/backoff-created.png';") == 1,
      "reopen catch-up should preserve note attachment refs while reconciling attachment create");
  sqlite3_close(db);

  require_single_note_attachment_ref_state(
      handle,
      "backoff-attachment-create.md",
      "assets/backoff-created.png",
      KERNEL_ATTACHMENT_PRESENCE_PRESENT,
      1,
      KERNEL_ATTACHMENT_KIND_IMAGE_LIKE,
      true,
      "reopen catch-up should expose closed-window attachment create through the formal public surface");
  require_attachment_lookup_state(
      handle,
      "assets/backoff-created.png",
      KERNEL_ATTACHMENT_PRESENCE_PRESENT,
      1,
      KERNEL_ATTACHMENT_KIND_IMAGE_LIKE,
      true,
      "reopen catch-up should expose created attachment metadata through the formal public surface");

  expect_ok(kernel_close(handle));
  std::filesystem::remove_all(vault);
  std::filesystem::remove_all(state_dir_for_vault(vault));
}

void test_close_during_watcher_fault_backoff_leaves_attachment_delete_for_reopen_catch_up() {
  const auto vault = make_temp_vault();
  const auto db_path = storage_db_for_vault(vault);
  std::filesystem::create_directories(vault / "assets");
  write_file_bytes(vault / "assets" / "backoff-delete.png", "attachment-bytes");

  kernel_handle* handle = nullptr;
  expect_ok(kernel_open_vault(vault.string().c_str(), &handle));

  const std::string content =
      "# Backoff Attachment Delete\n"
      "![Figure](assets/backoff-delete.png)\n";
  kernel_note_metadata metadata{};
  kernel_write_disposition disposition{};
  expect_ok(kernel_write_note(
      handle,
      "backoff-attachment-delete.md",
      content.data(),
      content.size(),
      nullptr,
      &metadata,
      &disposition));
  require_index_ready(handle, "watcher backoff attachment-delete test should start from READY");

  sqlite3* db = open_sqlite_readonly(db_path);
  require_true(
      query_single_int(
          db,
          "SELECT COUNT(*) FROM attachments "
          "WHERE rel_path='assets/backoff-delete.png' AND is_missing=0;") == 1,
      "seed write should register the present attachment before the closed-window delete");
  sqlite3_close(db);

  kernel::watcher::inject_next_poll_errors(handle->watcher_session, std::errc::io_error, 20);
  require_eventually(
      [&]() {
        kernel_state_snapshot snapshot{};
        return kernel_get_state(handle, &snapshot).code == KERNEL_OK &&
               snapshot.index_state == KERNEL_INDEX_UNAVAILABLE;
      },
      "watcher should enter UNAVAILABLE backoff before closed-window attachment delete");

  std::filesystem::remove(vault / "assets" / "backoff-delete.png");
  expect_ok(kernel_close(handle));

  db = open_sqlite_readonly(db_path);
  require_true(
      query_single_int(
          db,
          "SELECT COUNT(*) FROM attachments "
          "WHERE rel_path='assets/backoff-delete.png' AND is_missing=0;") == 1,
      "close during watcher backoff should leave the stale present attachment row for reopen catch-up to reconcile");
  sqlite3_close(db);

  handle = nullptr;
  expect_ok(kernel_open_vault(vault.string().c_str(), &handle));
  require_index_ready(handle, "reopen after watcher-backoff attachment delete should settle to READY");

  db = open_sqlite_readonly(db_path);
  require_true(
      query_single_int(
          db,
          "SELECT COUNT(*) FROM attachments "
          "WHERE rel_path='assets/backoff-delete.png' AND is_missing=1;") == 1,
      "reopen catch-up should reconcile a closed-window attachment delete back to missing state");
  require_true(
      query_single_int(
          db,
          "SELECT COUNT(*) FROM note_attachment_refs "
          "WHERE note_id=(SELECT note_id FROM notes WHERE rel_path='backoff-attachment-delete.md') "
          "AND attachment_rel_path='assets/backoff-delete.png';") == 1,
      "reopen catch-up should preserve note attachment refs while reconciling attachment delete");
  sqlite3_close(db);

  require_single_note_attachment_ref_state(
      handle,
      "backoff-attachment-delete.md",
      "assets/backoff-delete.png",
      KERNEL_ATTACHMENT_PRESENCE_MISSING,
      1,
      KERNEL_ATTACHMENT_KIND_IMAGE_LIKE,
      true,
      "reopen catch-up should expose closed-window attachment delete through the formal public surface");
  require_attachment_lookup_state(
      handle,
      "assets/backoff-delete.png",
      KERNEL_ATTACHMENT_PRESENCE_MISSING,
      1,
      KERNEL_ATTACHMENT_KIND_IMAGE_LIKE,
      true,
      "reopen catch-up should expose deleted attachment metadata through the formal public surface");

  expect_ok(kernel_close(handle));
  std::filesystem::remove_all(vault);
  std::filesystem::remove_all(state_dir_for_vault(vault));
}

void test_close_during_watcher_fault_backoff_leaves_attachment_modify_for_reopen_catch_up() {
  const auto vault = make_temp_vault();
  const auto db_path = storage_db_for_vault(vault);
  std::filesystem::create_directories(vault / "assets");
  const auto attachment_path = vault / "assets" / "backoff-modify.bin";
  write_file_bytes(attachment_path, "old-bytes");

  kernel_handle* handle = nullptr;
  expect_ok(kernel_open_vault(vault.string().c_str(), &handle));

  const std::string content =
      "# Backoff Attachment Modify\n"
      "![Figure](assets/backoff-modify.bin)\n";
  kernel_note_metadata metadata{};
  kernel_write_disposition disposition{};
  expect_ok(kernel_write_note(
      handle,
      "backoff-attachment-modify.md",
      content.data(),
      content.size(),
      nullptr,
      &metadata,
      &disposition));
  require_index_ready(handle, "watcher backoff attachment-modify test should start from READY");

  sqlite3* db = open_sqlite_readonly(db_path);
  const auto original_size = query_single_int(
      db,
      "SELECT file_size FROM attachments WHERE rel_path='assets/backoff-modify.bin';");
  sqlite3_close(db);

  kernel::watcher::inject_next_poll_errors(handle->watcher_session, std::errc::io_error, 20);
  require_eventually(
      [&]() {
        kernel_state_snapshot snapshot{};
        return kernel_get_state(handle, &snapshot).code == KERNEL_OK &&
               snapshot.index_state == KERNEL_INDEX_UNAVAILABLE;
      },
      "watcher should enter UNAVAILABLE backoff before closed-window attachment modify");

  write_file_bytes(attachment_path, "new-bytes-that-are-longer");
  expect_ok(kernel_close(handle));

  db = open_sqlite_readonly(db_path);
  require_true(
      query_single_int(
          db,
          "SELECT file_size FROM attachments WHERE rel_path='assets/backoff-modify.bin';") == original_size,
      "close during watcher backoff should leave stale attachment metadata for reopen catch-up to reconcile");
  sqlite3_close(db);

  handle = nullptr;
  expect_ok(kernel_open_vault(vault.string().c_str(), &handle));
  require_index_ready(handle, "reopen after watcher-backoff attachment modify should settle to READY");

  db = open_sqlite_readonly(db_path);
  require_true(
      query_single_int(
          db,
          "SELECT file_size FROM attachments WHERE rel_path='assets/backoff-modify.bin';") >
          original_size,
      "reopen catch-up should reconcile a closed-window attachment modify back to current metadata");
  require_true(
      query_single_int(
          db,
          "SELECT COUNT(*) FROM note_attachment_refs "
          "WHERE note_id=(SELECT note_id FROM notes WHERE rel_path='backoff-attachment-modify.md') "
          "AND attachment_rel_path='assets/backoff-modify.bin';") == 1,
      "reopen catch-up should preserve note attachment refs while reconciling attachment modify");
  sqlite3_close(db);

  require_single_note_attachment_ref_state(
      handle,
      "backoff-attachment-modify.md",
      "assets/backoff-modify.bin",
      KERNEL_ATTACHMENT_PRESENCE_PRESENT,
      1,
      KERNEL_ATTACHMENT_KIND_GENERIC_FILE,
      true,
      "reopen catch-up should expose closed-window attachment modify through the formal public surface");
  {
    kernel_attachment_record attachment{};
    expect_ok(kernel_get_attachment(handle, "assets/backoff-modify.bin", &attachment));
    require_true(
        attachment.presence == KERNEL_ATTACHMENT_PRESENCE_PRESENT,
        "reopen catch-up should keep modified attachment present in the formal public surface");
    require_true(
        attachment.kind == KERNEL_ATTACHMENT_KIND_GENERIC_FILE,
        "reopen catch-up should preserve modified attachment kind in the formal public surface");
    require_true(
        attachment.ref_count == 1,
        "reopen catch-up should preserve modified attachment ref_count in the formal public surface");
    require_true(
        attachment.file_size > static_cast<std::uint64_t>(original_size),
        "reopen catch-up should refresh modified attachment file_size in the formal public surface");
    require_true(
        attachment.mtime_ns > 0,
        "reopen catch-up should refresh modified attachment mtime in the formal public surface");
    kernel_free_attachment_record(&attachment);
  }

  expect_ok(kernel_close(handle));
  std::filesystem::remove_all(vault);
  std::filesystem::remove_all(state_dir_for_vault(vault));
}

void test_startup_recovery_marks_missing_attachments_for_recovered_note_refs() {
  const auto vault = make_temp_vault();
  const auto db_path = storage_db_for_vault(vault);
  const auto state_dir = state_dir_for_vault(vault);
  const auto journal_path = journal_path_for_vault(vault);
  const auto target_path = vault / "recover-missing-attachment.md";
  const auto temp_path = target_path.parent_path() / "recover-missing-attachment.md.tmp";

  kernel_handle* handle = nullptr;
  expect_ok(kernel_open_vault(vault.string().c_str(), &handle));

  const std::string original =
      "# Recover Missing Attachment\n"
      "![Old](assets/original.png)\n";
  kernel_note_metadata metadata{};
  kernel_write_disposition disposition{};
  expect_ok(kernel_write_note(
      handle,
      "recover-missing-attachment.md",
      original.data(),
      original.size(),
      nullptr,
      &metadata,
      &disposition));
  expect_ok(kernel_close(handle));

  write_file_bytes(
      target_path,
      "# Recover Missing Attachment\n"
      "![Missing](assets/missing-after-recovery.png)\n");
  write_file_bytes(temp_path, "stale-temp");
  require_true(
      kernel::recovery::append_save_begin(
          journal_path,
          "recover-missing-attachment-op",
          "recover-missing-attachment.md",
          temp_path)
          .value() == 0,
      "missing-attachment recovery SAVE_BEGIN append should succeed");

  expect_ok(kernel_open_vault(vault.string().c_str(), &handle));

  sqlite3* db = open_sqlite_readonly(db_path);
  require_true(
      query_single_int(
          db,
          "SELECT COUNT(*) FROM note_attachment_refs "
          "WHERE note_id=(SELECT note_id FROM notes WHERE rel_path='recover-missing-attachment.md');") == 1,
      "startup recovery should preserve the recovered note attachment ref even when the attachment is missing");
  require_true(
      query_single_text(
          db,
          "SELECT attachment_rel_path FROM note_attachment_refs "
          "WHERE note_id=(SELECT note_id FROM notes WHERE rel_path='recover-missing-attachment.md') LIMIT 1;") ==
          "assets/missing-after-recovery.png",
      "startup recovery should persist the recovered missing attachment ref");
  require_true(
      query_single_int(
          db,
          "SELECT COUNT(*) FROM attachments "
          "WHERE rel_path='assets/missing-after-recovery.png' AND is_missing=1;") == 1,
      "startup recovery should mark the recovered missing attachment path as missing");
  sqlite3_close(db);

  require_single_note_attachment_ref_state(
      handle,
      "recover-missing-attachment.md",
      "assets/missing-after-recovery.png",
      KERNEL_ATTACHMENT_PRESENCE_MISSING,
      1,
      KERNEL_ATTACHMENT_KIND_IMAGE_LIKE,
      false,
      "startup recovery should expose recovered missing attachment refs through the formal public surface");
  require_attachment_lookup_state(
      handle,
      "assets/missing-after-recovery.png",
      KERNEL_ATTACHMENT_PRESENCE_MISSING,
      1,
      KERNEL_ATTACHMENT_KIND_IMAGE_LIKE,
      false,
      "startup recovery should expose recovered missing attachment metadata through the formal public surface");

  expect_ok(kernel_close(handle));
  std::filesystem::remove_all(vault);
  std::filesystem::remove_all(state_dir);
}

void test_search_api_returns_matching_hits() {
  const auto vault = make_temp_vault();
  kernel_handle* handle = nullptr;
  expect_ok(kernel_open_vault(vault.string().c_str(), &handle));

  const std::string content =
      "# Search API Title\n"
      "Contains apisearchtoken in the body.\n";
  kernel_note_metadata metadata{};
  kernel_write_disposition disposition{};
  expect_ok(kernel_write_note(
      handle,
      "search-api.md",
      content.data(),
      content.size(),
      nullptr,
      &metadata,
      &disposition));

  kernel_search_results results{};
  expect_ok(kernel_search_notes(handle, "apisearchtoken", &results));
  require_true(results.count == 1, "search API should return one hit");
  require_true(std::string(results.hits[0].rel_path) == "search-api.md", "search API should preserve rel_path");
  require_true(std::string(results.hits[0].title) == "Search API Title", "search API should preserve title");
  kernel_free_search_results(&results);

  expect_ok(kernel_close(handle));
  std::filesystem::remove_all(vault);
  std::filesystem::remove_all(state_dir_for_vault(vault));
}

kernel_search_query make_default_search_query(const char* query, const std::size_t limit) {
  kernel_search_query request{};
  request.query = query;
  request.limit = limit;
  request.offset = 0;
  request.kind = KERNEL_SEARCH_KIND_NOTE;
  request.tag_filter = nullptr;
  request.path_prefix = nullptr;
  request.include_deleted = 0;
  request.sort_mode = KERNEL_SEARCH_SORT_REL_PATH_ASC;
  return request;
}

void test_expanded_search_api_returns_body_snippet_and_exact_total_hits() {
  const auto vault = make_temp_vault();
  kernel_handle* handle = nullptr;
  expect_ok(kernel_open_vault(vault.string().c_str(), &handle));

  kernel_note_metadata metadata{};
  kernel_write_disposition disposition{};
  const std::string a_text =
      "# A Expanded\n"
      "body contains ExpandedBodyToken near the first note\n";
  const std::string b_text =
      "# B Expanded\n"
      "body contains ExpandedBodyToken near the second note\n";
  expect_ok(kernel_write_note(handle, "b-expanded-body.md", b_text.data(), b_text.size(), nullptr, &metadata, &disposition));
  expect_ok(kernel_write_note(handle, "a-expanded-body.md", a_text.data(), a_text.size(), nullptr, &metadata, &disposition));

  kernel_search_query request = make_default_search_query("ExpandedBodyToken", 1);
  kernel_search_page page{};
  expect_ok(kernel_query_search(handle, &request, &page));
  require_true(page.count == 1, "expanded search should honor the page limit");
  require_true(page.total_hits == 2, "expanded search should report an exact total hit count from the same query snapshot");
  require_true(page.has_more == 1, "expanded search should report has_more when more hits remain");
  require_true(
      std::string(page.hits[0].rel_path) == "a-expanded-body.md",
      "expanded search should preserve rel_path ordering while ranking is not enabled");
  require_true(
      std::string(page.hits[0].title) == "A Expanded",
      "expanded search should preserve titles");
  require_true(
      std::string(page.hits[0].snippet).find("ExpandedBodyToken") != std::string::npos,
      "expanded search should expose a body snippet containing the matching token");
  require_true(
      page.hits[0].snippet_status == KERNEL_SEARCH_SNIPPET_BODY_EXTRACTED,
      "expanded search should report BODY_EXTRACTED when the body snippet is available");
  require_true(
      page.hits[0].result_kind == KERNEL_SEARCH_RESULT_NOTE,
      "expanded search Batch 1 should return note hits");
  require_true(
      page.hits[0].result_flags == KERNEL_SEARCH_RESULT_FLAG_NONE,
      "expanded search Batch 1 should not set result flags for notes");
  require_true(
      page.hits[0].match_flags == KERNEL_SEARCH_MATCH_BODY,
      "expanded search should preserve body match flags");
  require_true(
      std::string(page.hits[0].snippet).find('\n') == std::string::npos,
      "expanded search body snippets should be plain text");
  kernel_free_search_page(&page);

  kernel_search_results legacy_results{};
  expect_ok(kernel_search_notes_limited(handle, "ExpandedBodyToken", 1, &legacy_results));
  require_true(legacy_results.count == 1, "legacy limited search should remain supported");
  require_true(
      std::string(legacy_results.hits[0].rel_path) == "a-expanded-body.md",
      "legacy limited search should keep the old rel_path ordering");
  require_true(
      legacy_results.hits[0].match_flags == KERNEL_SEARCH_MATCH_BODY,
      "legacy limited search should keep the old match flag behavior");
  kernel_free_search_results(&legacy_results);

  expect_ok(kernel_close(handle));
  std::filesystem::remove_all(vault);
  std::filesystem::remove_all(state_dir_for_vault(vault));
}

void test_expanded_search_api_returns_title_only_without_snippet() {
  const auto vault = make_temp_vault();
  kernel_handle* handle = nullptr;
  expect_ok(kernel_open_vault(vault.string().c_str(), &handle));

  const std::string content =
      "# ExpandedTitleOnlyToken\n"
      "body text without the unique title token\n";
  kernel_note_metadata metadata{};
  kernel_write_disposition disposition{};
  expect_ok(kernel_write_note(
      handle,
      "expanded-title-only.md",
      content.data(),
      content.size(),
      nullptr,
      &metadata,
      &disposition));

  kernel_search_query request = make_default_search_query("ExpandedTitleOnlyToken", 10);
  kernel_search_page page{};
  expect_ok(kernel_query_search(handle, &request, &page));
  require_true(page.count == 1, "expanded title-only search should return one hit");
  require_true(page.total_hits == 1, "expanded title-only search should report one total hit");
  require_true(page.has_more == 0, "expanded title-only search should report no remaining pages");
  require_true(
      page.hits[0].snippet_status == KERNEL_SEARCH_SNIPPET_TITLE_ONLY,
      "expanded title-only search should report TITLE_ONLY when no body snippet exists");
  require_true(
      std::string(page.hits[0].snippet).empty(),
      "expanded title-only search should leave the snippet empty");
  require_true(
      page.hits[0].match_flags == KERNEL_SEARCH_MATCH_TITLE,
      "expanded title-only search should preserve title match flags");
  kernel_free_search_page(&page);

  expect_ok(kernel_close(handle));
  std::filesystem::remove_all(vault);
  std::filesystem::remove_all(state_dir_for_vault(vault));
}

void test_expanded_search_api_strips_title_heading_and_collapses_body_whitespace() {
  const auto vault = make_temp_vault();
  kernel_handle* handle = nullptr;
  expect_ok(kernel_open_vault(vault.string().c_str(), &handle));

  const std::string content =
      "# Expanded Snippet Title\n"
      "first line with    ExpandedSnippetToken\n"
      "\n"
      "second line after token\n";
  kernel_note_metadata metadata{};
  kernel_write_disposition disposition{};
  expect_ok(kernel_write_note(
      handle,
      "expanded-snippet.md",
      content.data(),
      content.size(),
      nullptr,
      &metadata,
      &disposition));

  kernel_search_query request = make_default_search_query("ExpandedSnippetToken", 10);
  kernel_search_page page{};
  expect_ok(kernel_query_search(handle, &request, &page));
  require_true(page.count == 1, "expanded snippet search should return one hit");
  require_true(
      std::string(page.hits[0].snippet).find("Expanded Snippet Title") == std::string::npos,
      "expanded snippet search should exclude the title heading from the body snippet");
  require_true(
      std::string(page.hits[0].snippet).find('\n') == std::string::npos,
      "expanded snippet search should collapse newlines");
  require_true(
      std::string(page.hits[0].snippet).find("  ") == std::string::npos,
      "expanded snippet search should collapse repeated whitespace");
  kernel_free_search_page(&page);

  expect_ok(kernel_close(handle));
  std::filesystem::remove_all(vault);
  std::filesystem::remove_all(state_dir_for_vault(vault));
}

void test_expanded_search_api_supports_note_tag_and_path_prefix_filters() {
  const auto vault = make_temp_vault();
  std::filesystem::create_directories(vault / "notes");
  std::filesystem::create_directories(vault / "misc");
  kernel_handle* handle = nullptr;
  expect_ok(kernel_open_vault(vault.string().c_str(), &handle));

  kernel_note_metadata metadata{};
  kernel_write_disposition disposition{};
  const std::string alpha =
      "# Alpha Filter\n"
      "#chem\n"
      "ExpandedFilterToken alpha\n";
  expect_ok(kernel_write_note(
      handle,
      "notes/alpha.md",
      alpha.data(),
      alpha.size(),
      nullptr,
      &metadata,
      &disposition));
  const std::string beta =
      "# Beta Filter\n"
      "#chem\n"
      "ExpandedFilterToken beta\n";
  expect_ok(kernel_write_note(
      handle,
      "notes/beta.md",
      beta.data(),
      beta.size(),
      nullptr,
      &metadata,
      &disposition));
  const std::string untagged =
      "# Untagged Filter\n"
      "ExpandedFilterToken untagged\n";
  expect_ok(kernel_write_note(
      handle,
      "notes/untagged.md",
      untagged.data(),
      untagged.size(),
      nullptr,
      &metadata,
      &disposition));
  const std::string outside_prefix =
      "# Outside Prefix\n"
      "#chem\n"
      "ExpandedFilterToken outside\n";
  expect_ok(kernel_write_note(
      handle,
      "misc/outside.md",
      outside_prefix.data(),
      outside_prefix.size(),
      nullptr,
      &metadata,
      &disposition));

  kernel_search_query request = make_default_search_query("ExpandedFilterToken", 10);
  request.tag_filter = "chem";
  request.path_prefix = "notes\\";
  kernel_search_page page{};
  expect_ok(kernel_query_search(handle, &request, &page));
  require_true(page.count == 2, "expanded search should return only tagged notes inside the prefix");
  require_true(page.total_hits == 2, "expanded search should report the exact filtered hit count");
  require_true(page.has_more == 0, "expanded filtered note search should report no remaining pages");
  require_true(std::string(page.hits[0].rel_path) == "notes/alpha.md", "expanded filtered note search should keep rel_path ordering");
  require_true(std::string(page.hits[1].rel_path) == "notes/beta.md", "expanded filtered note search should keep rel_path ordering");
  require_true(page.hits[0].result_kind == KERNEL_SEARCH_RESULT_NOTE, "expanded filtered note search should keep note result kind");
  kernel_free_search_page(&page);

  expect_ok(kernel_close(handle));
  std::filesystem::remove_all(vault);
  std::filesystem::remove_all(state_dir_for_vault(vault));
}

void test_expanded_search_api_supports_exact_offset_limit_pagination() {
  const auto vault = make_temp_vault();
  kernel_handle* handle = nullptr;
  expect_ok(kernel_open_vault(vault.string().c_str(), &handle));

  kernel_note_metadata metadata{};
  kernel_write_disposition disposition{};
  for (int index = 0; index < 5; ++index) {
    const std::string rel_path = "page-" + two_digit_index(index) + ".md";
    const std::string title = "# Page " + two_digit_index(index) + "\n";
    const std::string body = "ExpandedPageToken body " + std::to_string(index) + "\n";
    const std::string content = title + body;
    expect_ok(kernel_write_note(
        handle,
        rel_path.c_str(),
        content.data(),
        content.size(),
        nullptr,
        &metadata,
        &disposition));
  }

  kernel_search_query request = make_default_search_query("ExpandedPageToken", 2);
  kernel_search_page page{};
  expect_ok(kernel_query_search(handle, &request, &page));
  require_true(page.count == 2, "first expanded page should return two hits");
  require_true(page.total_hits == 5, "first expanded page should report the exact total hit count");
  require_true(page.has_more == 1, "first expanded page should report has_more");
  require_true(std::string(page.hits[0].rel_path) == "page-00.md", "first expanded page should start at the first rel_path");
  require_true(std::string(page.hits[1].rel_path) == "page-01.md", "first expanded page should preserve rel_path order");
  kernel_free_search_page(&page);

  request.offset = 2;
  expect_ok(kernel_query_search(handle, &request, &page));
  require_true(page.count == 2, "middle expanded page should return two hits");
  require_true(page.total_hits == 5, "middle expanded page should preserve the exact total hit count");
  require_true(page.has_more == 1, "middle expanded page should still report has_more");
  require_true(std::string(page.hits[0].rel_path) == "page-02.md", "middle expanded page should begin at the requested offset");
  require_true(std::string(page.hits[1].rel_path) == "page-03.md", "middle expanded page should preserve rel_path order");
  kernel_free_search_page(&page);

  request.offset = 4;
  expect_ok(kernel_query_search(handle, &request, &page));
  require_true(page.count == 1, "last expanded page should return the remaining hit only");
  require_true(page.total_hits == 5, "last expanded page should preserve the exact total hit count");
  require_true(page.has_more == 0, "last expanded page should report no remaining hits");
  require_true(std::string(page.hits[0].rel_path) == "page-04.md", "last expanded page should return the last rel_path");
  kernel_free_search_page(&page);

  request.offset = 9;
  expect_ok(kernel_query_search(handle, &request, &page));
  require_true(page.count == 0, "out-of-range expanded page should return no hits");
  require_true(page.total_hits == 5, "out-of-range expanded page should still expose the exact total hit count");
  require_true(page.has_more == 0, "out-of-range expanded page should report no remaining hits");
  kernel_free_search_page(&page);

  request.offset = 2;
  expect_ok(kernel_query_search(handle, &request, &page));
  require_true(page.count == 2, "repeated expanded page query should still return two hits");
  require_true(std::string(page.hits[0].rel_path) == "page-02.md", "repeated expanded page query should preserve a stable first hit");
  require_true(std::string(page.hits[1].rel_path) == "page-03.md", "repeated expanded page query should preserve a stable second hit");
  kernel_free_search_page(&page);

  expect_ok(kernel_close(handle));
  std::filesystem::remove_all(vault);
  std::filesystem::remove_all(state_dir_for_vault(vault));
}

void test_expanded_search_api_rejects_invalid_page_limits() {
  const auto vault = make_temp_vault();
  kernel_handle* handle = nullptr;
  expect_ok(kernel_open_vault(vault.string().c_str(), &handle));

  kernel_note_metadata metadata{};
  kernel_write_disposition disposition{};
  const std::string content =
      "# Page Limit Test\n"
      "ExpandedLimitToken\n";
  expect_ok(kernel_write_note(
      handle,
      "page-limit.md",
      content.data(),
      content.size(),
      nullptr,
      &metadata,
      &disposition));

  kernel_search_query request = make_default_search_query("ExpandedLimitToken", 1);
  kernel_search_page page{};
  expect_ok(kernel_query_search(handle, &request, &page));
  require_true(page.count == 1, "page-limit setup should return one hit");

  request.limit = 0;
  require_true(
      kernel_query_search(handle, &request, &page).code == KERNEL_ERROR_INVALID_ARGUMENT,
      "expanded search should reject zero page limits");
  require_true(
      page.hits == nullptr && page.count == 0 && page.total_hits == 0 && page.has_more == 0,
      "expanded search should clear stale output when zero page limit is rejected");

  request = make_default_search_query(
      "ExpandedLimitToken",
      kernel::search::kSearchPageMaxLimit + 1);
  require_true(
      kernel_query_search(handle, &request, &page).code == KERNEL_ERROR_INVALID_ARGUMENT,
      "expanded search should reject page limits above the frozen maximum");
  require_true(
      page.hits == nullptr && page.count == 0 && page.total_hits == 0 && page.has_more == 0,
      "expanded search should clear stale output when over-max page limits are rejected");

  expect_ok(kernel_close(handle));
  std::filesystem::remove_all(vault);
  std::filesystem::remove_all(state_dir_for_vault(vault));
}

void test_expanded_search_api_pagination_tracks_rewrite_and_rebuild() {
  const auto vault = make_temp_vault();
  kernel_handle* handle = nullptr;
  expect_ok(kernel_open_vault(vault.string().c_str(), &handle));

  kernel_note_metadata metadata{};
  kernel_write_disposition disposition{};
  for (int index = 0; index < 4; ++index) {
    const std::string rel_path = "lifecycle-" + two_digit_index(index) + ".md";
    const std::string content =
        "# Lifecycle " + two_digit_index(index) + "\nExpandedLifecycleToken " + std::to_string(index) + "\n";
    expect_ok(kernel_write_note(
        handle,
        rel_path.c_str(),
        content.data(),
        content.size(),
        nullptr,
        &metadata,
        &disposition));
  }

  kernel_search_query request = make_default_search_query("ExpandedLifecycleToken", 2);
  request.offset = 1;
  kernel_search_page page{};
  expect_ok(kernel_query_search(handle, &request, &page));
  require_true(page.count == 2, "pagination lifecycle setup should return a middle page");
  require_true(page.total_hits == 4, "pagination lifecycle setup should report four hits");
  require_true(std::string(page.hits[0].rel_path) == "lifecycle-01.md", "pagination lifecycle setup should expose the second hit at offset one");
  kernel_free_search_page(&page);

  kernel_owned_buffer existing_note{};
  kernel_note_metadata existing_metadata{};
  expect_ok(kernel_read_note(handle, "lifecycle-01.md", &existing_note, &existing_metadata));
  kernel_free_buffer(&existing_note);

  const std::string rewritten =
      "# Lifecycle 01\nno shared token now\n";
  expect_ok(kernel_write_note(
      handle,
      "lifecycle-01.md",
      rewritten.data(),
      rewritten.size(),
      existing_metadata.content_revision,
      &metadata,
      &disposition));

  expect_ok(kernel_query_search(handle, &request, &page));
  require_true(page.count == 2, "pagination should still return a full page after rewrite");
  require_true(page.total_hits == 3, "pagination should track the exact hit count after rewrite");
  require_true(std::string(page.hits[0].rel_path) == "lifecycle-02.md", "pagination should advance to the next rel_path after a rewrite removes one hit");
  kernel_free_search_page(&page);

  {
    std::lock_guard lock(handle->storage_mutex);
    exec_sql(
        handle->storage.connection,
        "DELETE FROM note_fts WHERE rowid=(SELECT note_id FROM notes WHERE rel_path='lifecycle-00.md');");
  }

  require_index_ready(handle, "pagination rebuild test should wait for READY before triggering rebuild");
  expect_ok(kernel_rebuild_index(handle));

  request.offset = 0;
  expect_ok(kernel_query_search(handle, &request, &page));
  require_true(page.count == 2, "pagination should return the first rebuilt page");
  require_true(page.total_hits == 3, "pagination should restore the exact hit count after rebuild repairs drift");
  require_true(std::string(page.hits[0].rel_path) == "lifecycle-00.md", "pagination should restore rebuilt hits back into the first page");
  require_true(page.has_more == 1, "pagination should still report has_more after rebuild when more hits remain");
  kernel_free_search_page(&page);

  expect_ok(kernel_close(handle));
  std::filesystem::remove_all(vault);
  std::filesystem::remove_all(state_dir_for_vault(vault));
}

void test_expanded_search_api_supports_attachment_path_hits_and_missing_flag() {
  const auto vault = make_temp_vault();
  std::filesystem::create_directories(vault / "assets");
  std::filesystem::create_directories(vault / "docs");
  write_file_bytes(vault / "assets" / "diagram.png", "png-bytes");
  write_file_bytes(vault / "docs" / "report.pdf", "pdf-bytes");

  kernel_handle* handle = nullptr;
  expect_ok(kernel_open_vault(vault.string().c_str(), &handle));

  const std::string content =
      "# Attachment Search\n"
      "![Figure](assets/diagram.png)\n"
      "[Report](docs/report.pdf)\n";
  kernel_note_metadata metadata{};
  kernel_write_disposition disposition{};
  expect_ok(kernel_write_note(
      handle,
      "attachment-search.md",
      content.data(),
      content.size(),
      nullptr,
      &metadata,
      &disposition));
  expect_ok(kernel_close(handle));

  std::filesystem::remove(vault / "docs" / "report.pdf");

  expect_ok(kernel_open_vault(vault.string().c_str(), &handle));
  require_index_ready(handle, "attachment search should wait for catch-up before querying missing attachment state");

  kernel_search_query request = make_default_search_query("report", 10);
  request.kind = KERNEL_SEARCH_KIND_ATTACHMENT;
  request.path_prefix = "docs\\";
  kernel_search_page page{};
  expect_ok(kernel_query_search(handle, &request, &page));
  require_true(page.count == 1, "expanded attachment search should return the matching attachment path");
  require_true(page.total_hits == 1, "expanded attachment search should report the exact total hit count");
  require_true(page.has_more == 0, "expanded attachment search should report no remaining pages");
  require_true(
      std::string(page.hits[0].rel_path) == "docs/report.pdf",
      "expanded attachment search should preserve the attachment rel_path");
  require_true(
      std::string(page.hits[0].title) == "report.pdf",
      "expanded attachment search should expose the attachment basename as title");
  require_true(
      std::string(page.hits[0].snippet).empty(),
      "expanded attachment search should not emit a snippet");
  require_true(
      page.hits[0].snippet_status == KERNEL_SEARCH_SNIPPET_NONE,
      "expanded attachment search should report no snippet state");
  require_true(
      page.hits[0].match_flags == KERNEL_SEARCH_MATCH_PATH,
      "expanded attachment search should report PATH matches");
  require_true(
      page.hits[0].result_kind == KERNEL_SEARCH_RESULT_ATTACHMENT,
      "expanded attachment search should mark the hit as an attachment");
  require_true(
      page.hits[0].result_flags == KERNEL_SEARCH_RESULT_FLAG_ATTACHMENT_MISSING,
      "expanded attachment search should surface missing attachment state");
  kernel_free_search_page(&page);

  expect_ok(kernel_close(handle));
  std::filesystem::remove_all(vault);
  std::filesystem::remove_all(state_dir_for_vault(vault));
}

void test_expanded_search_api_supports_kind_all_notes_first_then_attachments() {
  const auto vault = make_temp_vault();
  std::filesystem::create_directories(vault / "all");
  write_file_bytes(vault / "all" / "expandedmixedtoken-00.png", "png-00");
  write_file_bytes(vault / "all" / "expandedmixedtoken-01.png", "png-01");

  kernel_handle* handle = nullptr;
  expect_ok(kernel_open_vault(vault.string().c_str(), &handle));

  kernel_note_metadata metadata{};
  kernel_write_disposition disposition{};
  const std::string first =
      "# A Mixed\n"
      "#chem\n"
      "ExpandedMixedToken first note body\n"
      "![Figure](all/expandedmixedtoken-00.png)\n";
  const std::string second =
      "# B Mixed\n"
      "#chem\n"
      "ExpandedMixedToken second note body\n"
      "![Figure](all/expandedmixedtoken-01.png)\n";
  expect_ok(kernel_write_note(
      handle,
      "all/a-note.md",
      first.data(),
      first.size(),
      nullptr,
      &metadata,
      &disposition));
  expect_ok(kernel_write_note(
      handle,
      "all/b-note.md",
      second.data(),
      second.size(),
      nullptr,
      &metadata,
      &disposition));

  kernel_search_query request = make_default_search_query("ExpandedMixedToken", 10);
  request.kind = KERNEL_SEARCH_KIND_ALL;
  request.tag_filter = "chem";
  request.path_prefix = "all/";
  kernel_search_page page{};
  expect_ok(kernel_query_search(handle, &request, &page));
  require_true(page.count == 4, "expanded all-kind search should return both notes and both attachments");
  require_true(page.total_hits == 4, "expanded all-kind search should report the exact combined hit count");
  require_true(page.has_more == 0, "expanded all-kind search should report no remaining pages on the full result");
  require_true(std::string(page.hits[0].rel_path) == "all/a-note.md", "expanded all-kind search should list notes first");
  require_true(std::string(page.hits[1].rel_path) == "all/b-note.md", "expanded all-kind search should preserve note rel_path order");
  require_true(std::string(page.hits[2].rel_path) == "all/expandedmixedtoken-00.png", "expanded all-kind search should place attachments after notes");
  require_true(std::string(page.hits[3].rel_path) == "all/expandedmixedtoken-01.png", "expanded all-kind search should preserve attachment rel_path order");
  require_true(page.hits[0].result_kind == KERNEL_SEARCH_RESULT_NOTE, "expanded all-kind search should tag note hits correctly");
  require_true(page.hits[2].result_kind == KERNEL_SEARCH_RESULT_ATTACHMENT, "expanded all-kind search should tag attachment hits correctly");
  kernel_free_search_page(&page);

  request.limit = 2;
  request.offset = 1;
  expect_ok(kernel_query_search(handle, &request, &page));
  require_true(page.count == 2, "expanded all-kind pagination should return a cross-boundary page");
  require_true(page.total_hits == 4, "expanded all-kind pagination should keep the exact combined hit count");
  require_true(page.has_more == 1, "expanded all-kind pagination should report more hits after the cross-boundary page");
  require_true(std::string(page.hits[0].rel_path) == "all/b-note.md", "expanded all-kind pagination should start at the requested offset");
  require_true(std::string(page.hits[1].rel_path) == "all/expandedmixedtoken-00.png", "expanded all-kind pagination should keep notes-first ordering");
  kernel_free_search_page(&page);

  expect_ok(kernel_close(handle));
  std::filesystem::remove_all(vault);
  std::filesystem::remove_all(state_dir_for_vault(vault));
}

void test_expanded_search_api_rejects_invalid_filter_and_ranking_combinations_and_clears_stale_output() {
  const auto vault = make_temp_vault();
  std::filesystem::create_directories(vault / "docs");
  write_file_bytes(vault / "docs" / "report.pdf", "pdf-bytes");

  kernel_handle* handle = nullptr;
  expect_ok(kernel_open_vault(vault.string().c_str(), &handle));

  const std::string content =
      "# Filter Boundary\n"
      "#chem\n"
      "ExpandedBoundaryToken\n"
      "[Report](docs/report.pdf)\n";
  kernel_note_metadata metadata{};
  kernel_write_disposition disposition{};
  expect_ok(kernel_write_note(
      handle,
      "filter-boundary.md",
      content.data(),
      content.size(),
      nullptr,
      &metadata,
      &disposition));

  kernel_search_query request = make_default_search_query("ExpandedBoundaryToken", 10);
  kernel_search_page page{};
  expect_ok(kernel_query_search(handle, &request, &page));
  require_true(page.count == 1, "expanded filter boundary setup should return one note hit");

  request = make_default_search_query("report", 10);
  request.kind = KERNEL_SEARCH_KIND_ATTACHMENT;
  request.tag_filter = "chem";
  require_true(
      kernel_query_search(handle, &request, &page).code == KERNEL_ERROR_INVALID_ARGUMENT,
      "expanded search should reject tag filters on attachment-only queries");
  require_true(
      page.hits == nullptr && page.count == 0 && page.total_hits == 0 && page.has_more == 0,
      "expanded search should clear stale output after rejecting attachment-plus-tag");

  request = make_default_search_query("ExpandedBoundaryToken", 10);
  request.path_prefix = "../notes/";
  require_true(
      kernel_query_search(handle, &request, &page).code == KERNEL_ERROR_INVALID_ARGUMENT,
      "expanded search should reject invalid relative path prefixes");
  require_true(
      page.hits == nullptr && page.count == 0 && page.total_hits == 0 && page.has_more == 0,
      "expanded search should clear stale output after rejecting invalid path prefixes");

  request = make_default_search_query("ExpandedBoundaryToken", 10);
  request.include_deleted = 1;
  require_true(
      kernel_query_search(handle, &request, &page).code == KERNEL_ERROR_INVALID_ARGUMENT,
      "expanded search should keep include_deleted disabled in Batch 4");

  request = make_default_search_query("report", 10);
  request.kind = KERNEL_SEARCH_KIND_ATTACHMENT;
  request.sort_mode = KERNEL_SEARCH_SORT_RANK_V1;
  require_true(
      kernel_query_search(handle, &request, &page).code == KERNEL_ERROR_INVALID_ARGUMENT,
      "expanded search should reject attachment-only Ranking v1 requests");

  kernel_search_results legacy_results{};
  expect_ok(kernel_search_notes(handle, "ExpandedBoundaryToken", &legacy_results));
  require_true(legacy_results.count == 1, "legacy search should remain supported after invalid expanded-search requests");
  kernel_free_search_results(&legacy_results);

  expect_ok(kernel_close(handle));
  std::filesystem::remove_all(vault);
  std::filesystem::remove_all(state_dir_for_vault(vault));
}

void test_expanded_search_api_filters_track_rewrite_and_rebuild() {
  const auto vault = make_temp_vault();
  std::filesystem::create_directories(vault / "filter-life");
  kernel_handle* handle = nullptr;
  expect_ok(kernel_open_vault(vault.string().c_str(), &handle));

  kernel_note_metadata metadata{};
  kernel_write_disposition disposition{};
  const std::string first =
      "# Filter Life A\n"
      "#chem\n"
      "ExpandedFilterLifeToken first\n";
  const std::string second =
      "# Filter Life B\n"
      "#chem\n"
      "ExpandedFilterLifeToken second\n";
  expect_ok(kernel_write_note(
      handle,
      "filter-life/a.md",
      first.data(),
      first.size(),
      nullptr,
      &metadata,
      &disposition));
  kernel_note_metadata second_metadata{};
  expect_ok(kernel_write_note(
      handle,
      "filter-life/b.md",
      second.data(),
      second.size(),
      nullptr,
      &second_metadata,
      &disposition));

  kernel_search_query request = make_default_search_query("ExpandedFilterLifeToken", 10);
  request.tag_filter = "chem";
  request.path_prefix = "filter-life/";
  kernel_search_page page{};
  expect_ok(kernel_query_search(handle, &request, &page));
  require_true(page.count == 2, "filtered lifecycle setup should return both tagged notes");
  require_true(page.total_hits == 2, "filtered lifecycle setup should report two exact hits");
  kernel_free_search_page(&page);

  const std::string rewritten =
      "# Filter Life B\n"
      "ExpandedFilterLifeToken second without tag\n";
  expect_ok(kernel_write_note(
      handle,
      "filter-life/b.md",
      rewritten.data(),
      rewritten.size(),
      second_metadata.content_revision,
      &metadata,
      &disposition));

  expect_ok(kernel_query_search(handle, &request, &page));
  require_true(page.count == 1, "filtered search should drop notes that lose the tag on rewrite");
  require_true(page.total_hits == 1, "filtered search should keep the exact hit count after rewrite");
  require_true(std::string(page.hits[0].rel_path) == "filter-life/a.md", "filtered search should keep the remaining tagged note");
  kernel_free_search_page(&page);

  {
    std::lock_guard lock(handle->storage_mutex);
    exec_sql(
        handle->storage.connection,
        "DELETE FROM note_tags "
        "WHERE note_id=(SELECT note_id FROM notes WHERE rel_path='filter-life/a.md');");
  }

  expect_ok(kernel_query_search(handle, &request, &page));
  require_true(page.count == 0, "manual tag drift should empty the filtered result before rebuild");
  require_true(page.total_hits == 0, "manual tag drift should drop the filtered hit count before rebuild");
  kernel_free_search_page(&page);

  require_index_ready(handle, "filtered lifecycle rebuild test should wait for READY before rebuilding");
  expect_ok(kernel_rebuild_index(handle));

  expect_ok(kernel_query_search(handle, &request, &page));
  require_true(page.count == 1, "rebuild should restore filtered search results after derived tag drift");
  require_true(page.total_hits == 1, "rebuild should restore the exact filtered hit count after drift");
  require_true(std::string(page.hits[0].rel_path) == "filter-life/a.md", "rebuild should restore the surviving tagged note");
  kernel_free_search_page(&page);

  expect_ok(kernel_close(handle));
  std::filesystem::remove_all(vault);
  std::filesystem::remove_all(state_dir_for_vault(vault));
}

void test_expanded_search_api_supports_note_ranking_v1_title_boost() {
  const auto vault = make_temp_vault();
  kernel_handle* handle = nullptr;
  expect_ok(kernel_open_vault(vault.string().c_str(), &handle));

  kernel_note_metadata metadata{};
  kernel_write_disposition disposition{};
  const std::string body_only =
      "# Generic Body Rank\n"
      "ExpandedRankToken appears only in the body\n";
  const std::string title_hit =
      "# ExpandedRankToken\n"
      "body text without the unique rank token\n";
  expect_ok(kernel_write_note(
      handle,
      "a-body-rank.md",
      body_only.data(),
      body_only.size(),
      nullptr,
      &metadata,
      &disposition));
  expect_ok(kernel_write_note(
      handle,
      "z-title-rank.md",
      title_hit.data(),
      title_hit.size(),
      nullptr,
      &metadata,
      &disposition));

  kernel_search_query request = make_default_search_query("ExpandedRankToken", 10);
  request.sort_mode = KERNEL_SEARCH_SORT_RANK_V1;
  kernel_search_page page{};
  expect_ok(kernel_query_search(handle, &request, &page));
  require_true(page.count == 2, "expanded Ranking v1 note search should return both matching notes");
  require_true(page.total_hits == 2, "expanded Ranking v1 note search should preserve the exact total hit count");
  require_true(std::string(page.hits[0].rel_path) == "z-title-rank.md", "expanded Ranking v1 note search should boost title hits ahead of body-only hits");
  require_true(std::string(page.hits[1].rel_path) == "a-body-rank.md", "expanded Ranking v1 note search should keep the body-only hit behind the title hit");
  require_true(
      (page.hits[0].match_flags & KERNEL_SEARCH_MATCH_TITLE) != 0,
      "expanded Ranking v1 note search should keep title match flags on the boosted title hit");
  require_true(
      page.hits[1].match_flags == KERNEL_SEARCH_MATCH_BODY,
      "expanded Ranking v1 note search should keep body-only match flags on the trailing body hit");
  kernel_free_search_page(&page);

  expect_ok(kernel_close(handle));
  std::filesystem::remove_all(vault);
  std::filesystem::remove_all(state_dir_for_vault(vault));
}

void test_expanded_search_api_supports_note_ranking_v1_single_token_tag_boost() {
  const auto vault = make_temp_vault();
  kernel_handle* handle = nullptr;
  expect_ok(kernel_open_vault(vault.string().c_str(), &handle));

  kernel_note_metadata metadata{};
  kernel_write_disposition disposition{};
  const std::string tagged =
      "# Tagged Rank\n"
      "#rankboost\n"
      "rankboost body token\n";
  const std::string untagged =
      "# Untagged Rank\n"
      "rankboost body token\n";
  expect_ok(kernel_write_note(
      handle,
      "b-tagged-rank.md",
      tagged.data(),
      tagged.size(),
      nullptr,
      &metadata,
      &disposition));
  expect_ok(kernel_write_note(
      handle,
      "a-untagged-rank.md",
      untagged.data(),
      untagged.size(),
      nullptr,
      &metadata,
      &disposition));

  kernel_search_query request = make_default_search_query("rankboost", 10);
  request.sort_mode = KERNEL_SEARCH_SORT_RANK_V1;
  kernel_search_page page{};
  expect_ok(kernel_query_search(handle, &request, &page));
  require_true(page.count == 2, "expanded Ranking v1 tag-boost search should return both matching notes");
  require_true(std::string(page.hits[0].rel_path) == "b-tagged-rank.md", "expanded Ranking v1 tag-boost search should boost exact single-token tag matches ahead of plain body matches");
  require_true(std::string(page.hits[1].rel_path) == "a-untagged-rank.md", "expanded Ranking v1 tag-boost search should leave the untagged note behind the boosted note");
  kernel_free_search_page(&page);

  expect_ok(kernel_close(handle));
  std::filesystem::remove_all(vault);
  std::filesystem::remove_all(state_dir_for_vault(vault));
}

void test_expanded_search_api_supports_kind_all_ranking_on_note_branch_only() {
  const auto vault = make_temp_vault();
  std::filesystem::create_directories(vault / "rankall");
  write_file_bytes(vault / "rankall" / "expandedallranktoken-00.png", "png-00");
  write_file_bytes(vault / "rankall" / "expandedallranktoken-01.png", "png-01");

  kernel_handle* handle = nullptr;
  expect_ok(kernel_open_vault(vault.string().c_str(), &handle));

  kernel_note_metadata metadata{};
  kernel_write_disposition disposition{};
  const std::string body_note =
      "# Generic Rank All\n"
      "ExpandedAllRankToken body match only\n"
      "![Figure](rankall/expandedallranktoken-00.png)\n";
  const std::string title_note =
      "# ExpandedAllRankToken\n"
      "body text without the unique rank token\n"
      "![Figure](rankall/expandedallranktoken-01.png)\n";
  expect_ok(kernel_write_note(
      handle,
      "rankall/a-body-note.md",
      body_note.data(),
      body_note.size(),
      nullptr,
      &metadata,
      &disposition));
  expect_ok(kernel_write_note(
      handle,
      "rankall/z-title-note.md",
      title_note.data(),
      title_note.size(),
      nullptr,
      &metadata,
      &disposition));

  kernel_search_query request = make_default_search_query("ExpandedAllRankToken", 10);
  request.kind = KERNEL_SEARCH_KIND_ALL;
  request.path_prefix = "rankall/";
  request.sort_mode = KERNEL_SEARCH_SORT_RANK_V1;
  kernel_search_page page{};
  expect_ok(kernel_query_search(handle, &request, &page));
  require_true(page.count == 4, "expanded Ranking v1 all-kind search should return both notes and both attachments");
  require_true(page.total_hits == 4, "expanded Ranking v1 all-kind search should report the exact combined hit count");
  require_true(std::string(page.hits[0].rel_path) == "rankall/z-title-note.md", "expanded Ranking v1 all-kind search should rank the note branch before attachments");
  require_true(std::string(page.hits[1].rel_path) == "rankall/a-body-note.md", "expanded Ranking v1 all-kind search should keep lower-ranked notes before attachments");
  require_true(std::string(page.hits[2].rel_path) == "rankall/expandedallranktoken-00.png", "expanded Ranking v1 all-kind search should append attachments after ranked notes");
  require_true(std::string(page.hits[3].rel_path) == "rankall/expandedallranktoken-01.png", "expanded Ranking v1 all-kind search should preserve attachment rel_path order");
  kernel_free_search_page(&page);

  request.limit = 2;
  request.offset = 1;
  expect_ok(kernel_query_search(handle, &request, &page));
  require_true(page.count == 2, "expanded Ranking v1 all-kind pagination should return a cross-boundary page");
  require_true(std::string(page.hits[0].rel_path) == "rankall/a-body-note.md", "expanded Ranking v1 all-kind pagination should continue from the ranked note branch");
  require_true(std::string(page.hits[1].rel_path) == "rankall/expandedallranktoken-00.png", "expanded Ranking v1 all-kind pagination should enter the attachment branch after notes");
  kernel_free_search_page(&page);

  expect_ok(kernel_close(handle));
  std::filesystem::remove_all(vault);
  std::filesystem::remove_all(state_dir_for_vault(vault));
}

void test_expanded_search_api_ranking_tracks_rewrite_and_rebuild() {
  const auto vault = make_temp_vault();
  kernel_handle* handle = nullptr;
  expect_ok(kernel_open_vault(vault.string().c_str(), &handle));

  kernel_note_metadata metadata{};
  kernel_write_disposition disposition{};
  const std::string title_hit =
      "# ExpandedRankLifecycleToken\n"
      "body text without the unique rank token\n";
  const std::string body_only =
      "# Generic Rank Lifecycle\n"
      "ExpandedRankLifecycleToken body only\n";
  expect_ok(kernel_write_note(
      handle,
      "z-rank-lifecycle-title.md",
      title_hit.data(),
      title_hit.size(),
      nullptr,
      &metadata,
      &disposition));
  kernel_note_metadata body_metadata{};
  expect_ok(kernel_write_note(
      handle,
      "a-rank-lifecycle-body.md",
      body_only.data(),
      body_only.size(),
      nullptr,
      &body_metadata,
      &disposition));

  kernel_search_query request = make_default_search_query("ExpandedRankLifecycleToken", 10);
  request.sort_mode = KERNEL_SEARCH_SORT_RANK_V1;
  kernel_search_page page{};
  expect_ok(kernel_query_search(handle, &request, &page));
  require_true(page.count == 2, "ranking lifecycle setup should return both matching notes");
  require_true(std::string(page.hits[0].rel_path) == "z-rank-lifecycle-title.md", "ranking lifecycle setup should start with the title-boosted note");
  kernel_free_search_page(&page);

  kernel_owned_buffer existing_note{};
  kernel_note_metadata existing_metadata{};
  expect_ok(kernel_read_note(handle, "z-rank-lifecycle-title.md", &existing_note, &existing_metadata));
  kernel_free_buffer(&existing_note);

  const std::string rewritten =
      "# Generic Rank Lifecycle\n"
      "body text without the rank token\n";
  expect_ok(kernel_write_note(
      handle,
      "z-rank-lifecycle-title.md",
      rewritten.data(),
      rewritten.size(),
      existing_metadata.content_revision,
      &metadata,
      &disposition));

  expect_ok(kernel_query_search(handle, &request, &page));
  require_true(page.count == 1, "ranking lifecycle should drop the removed title hit after rewrite");
  require_true(page.total_hits == 1, "ranking lifecycle should keep the exact hit count after rewrite");
  require_true(std::string(page.hits[0].rel_path) == "a-rank-lifecycle-body.md", "ranking lifecycle should leave the surviving body hit first after rewrite");
  kernel_free_search_page(&page);

  {
    std::lock_guard lock(handle->storage_mutex);
    exec_sql(
        handle->storage.connection,
        "DELETE FROM note_fts WHERE rowid=(SELECT note_id FROM notes WHERE rel_path='a-rank-lifecycle-body.md');");
  }

  expect_ok(kernel_query_search(handle, &request, &page));
  require_true(page.count == 0, "ranking lifecycle should observe missing FTS rows before rebuild repairs them");
  require_true(page.total_hits == 0, "ranking lifecycle should observe zero exact hits before rebuild repairs drift");
  kernel_free_search_page(&page);

  require_index_ready(handle, "ranking lifecycle rebuild test should wait for READY before rebuilding");
  expect_ok(kernel_rebuild_index(handle));

  expect_ok(kernel_query_search(handle, &request, &page));
  require_true(page.count == 1, "ranking lifecycle should restore the surviving hit after rebuild");
  require_true(page.total_hits == 1, "ranking lifecycle should restore the exact hit count after rebuild");
  require_true(std::string(page.hits[0].rel_path) == "a-rank-lifecycle-body.md", "ranking lifecycle should restore the surviving body hit after rebuild");
  kernel_free_search_page(&page);

  expect_ok(kernel_close(handle));
  std::filesystem::remove_all(vault);
  std::filesystem::remove_all(state_dir_for_vault(vault));
}

void test_search_api_rewrite_replaces_old_hits() {
  const auto vault = make_temp_vault();
  kernel_handle* handle = nullptr;
  expect_ok(kernel_open_vault(vault.string().c_str(), &handle));

  const std::string original =
      "# Search Rewrite\n"
      "alphaapitoken\n";
  kernel_note_metadata first{};
  kernel_write_disposition disposition{};
  expect_ok(kernel_write_note(
      handle,
      "search-rewrite.md",
      original.data(),
      original.size(),
      nullptr,
      &first,
      &disposition));

  const std::string rewritten =
      "# Search Rewrite\n"
      "betaapitoken\n";
  kernel_note_metadata second{};
  expect_ok(kernel_write_note(
      handle,
      "search-rewrite.md",
      rewritten.data(),
      rewritten.size(),
      first.content_revision,
      &second,
      &disposition));

  kernel_search_results results{};
  expect_ok(kernel_search_notes(handle, "alphaapitoken", &results));
  require_true(results.count == 0, "rewrite should remove old search hits");
  kernel_free_search_results(&results);

  expect_ok(kernel_search_notes(handle, "betaapitoken", &results));
  require_true(results.count == 1, "rewrite should keep the new search hit");
  require_true(std::string(results.hits[0].rel_path) == "search-rewrite.md", "rewrite hit should preserve rel_path");
  kernel_free_search_results(&results);

  expect_ok(kernel_close(handle));
  std::filesystem::remove_all(vault);
  std::filesystem::remove_all(state_dir_for_vault(vault));
}

void test_search_api_accepts_hyphenated_literal_query() {
  const auto vault = make_temp_vault();
  kernel_handle* handle = nullptr;
  expect_ok(kernel_open_vault(vault.string().c_str(), &handle));

  const std::string content =
      "# Search API Hyphen\n"
      "Contains api-search-token in the body.\n";
  kernel_note_metadata metadata{};
  kernel_write_disposition disposition{};
  expect_ok(kernel_write_note(
      handle,
      "search-hyphen.md",
      content.data(),
      content.size(),
      nullptr,
      &metadata,
      &disposition));

  kernel_search_results results{};
  expect_ok(kernel_search_notes(handle, "api-search-token", &results));
  require_true(results.count == 1, "hyphenated search API query should return one hit");
  require_true(std::string(results.hits[0].rel_path) == "search-hyphen.md", "hyphenated search hit should preserve rel_path");
  kernel_free_search_results(&results);

  expect_ok(kernel_close(handle));
  std::filesystem::remove_all(vault);
  std::filesystem::remove_all(state_dir_for_vault(vault));
}

void test_search_api_rejects_whitespace_only_query() {
  const auto vault = make_temp_vault();
  kernel_handle* handle = nullptr;
  expect_ok(kernel_open_vault(vault.string().c_str(), &handle));

  const std::string content =
      "# Search API Whitespace\n"
      "Contains stabletoken in the body.\n";
  kernel_note_metadata metadata{};
  kernel_write_disposition disposition{};
  expect_ok(kernel_write_note(
      handle,
      "search-whitespace.md",
      content.data(),
      content.size(),
      nullptr,
      &metadata,
      &disposition));

  kernel_search_results results{};
  const kernel_status status = kernel_search_notes(handle, "   \t  ", &results);
  require_true(status.code == KERNEL_ERROR_INVALID_ARGUMENT, "whitespace-only search API query should be invalid");
  require_true(results.count == 0, "invalid search API query should not return hits");
  require_true(results.hits == nullptr, "invalid search API query should not allocate hits");
  kernel_free_search_results(&results);

  expect_ok(kernel_close(handle));
  std::filesystem::remove_all(vault);
  std::filesystem::remove_all(state_dir_for_vault(vault));
}

void test_search_api_matches_multiple_literal_tokens_with_extra_whitespace() {
  const auto vault = make_temp_vault();
  kernel_handle* handle = nullptr;
  expect_ok(kernel_open_vault(vault.string().c_str(), &handle));

  const std::string content =
      "# Search API Multi Token\n"
      "Contains alpha-token and beta-token in the body.\n";
  kernel_note_metadata metadata{};
  kernel_write_disposition disposition{};
  expect_ok(kernel_write_note(
      handle,
      "search-multi-token.md",
      content.data(),
      content.size(),
      nullptr,
      &metadata,
      &disposition));

  kernel_search_results results{};
  expect_ok(kernel_search_notes(handle, "  alpha-token   beta-token  ", &results));
  require_true(results.count == 1, "multi-token search API query should return one hit");
  require_true(std::string(results.hits[0].rel_path) == "search-multi-token.md", "multi-token search hit should preserve rel_path");
  kernel_free_search_results(&results);

  expect_ok(kernel_close(handle));
  std::filesystem::remove_all(vault);
  std::filesystem::remove_all(state_dir_for_vault(vault));
}

void test_search_api_returns_hits_in_rel_path_order() {
  const auto vault = make_temp_vault();
  kernel_handle* handle = nullptr;
  expect_ok(kernel_open_vault(vault.string().c_str(), &handle));

  const std::string content_b =
      "# B Title\n"
      "shared-order-token\n";
  kernel_note_metadata metadata_b{};
  kernel_write_disposition disposition{};
  expect_ok(kernel_write_note(
      handle,
      "b-note.md",
      content_b.data(),
      content_b.size(),
      nullptr,
      &metadata_b,
      &disposition));

  const std::string content_a =
      "# A Title\n"
      "shared-order-token\n";
  kernel_note_metadata metadata_a{};
  expect_ok(kernel_write_note(
      handle,
      "a-note.md",
      content_a.data(),
      content_a.size(),
      nullptr,
      &metadata_a,
      &disposition));

  kernel_search_results results{};
  expect_ok(kernel_search_notes(handle, "shared-order-token", &results));
  require_true(results.count == 2, "ordered search API query should return two hits");
  require_true(std::string(results.hits[0].rel_path) == "a-note.md", "search API hits should be ordered by rel_path ascending");
  require_true(std::string(results.hits[1].rel_path) == "b-note.md", "search API hits should be ordered by rel_path ascending");
  kernel_free_search_results(&results);

  expect_ok(kernel_close(handle));
  std::filesystem::remove_all(vault);
  std::filesystem::remove_all(state_dir_for_vault(vault));
}

void test_search_api_returns_one_hit_per_note_even_with_repeated_term() {
  const auto vault = make_temp_vault();
  kernel_handle* handle = nullptr;
  expect_ok(kernel_open_vault(vault.string().c_str(), &handle));

  const std::string content =
      "# Repeated API Term\n"
      "repeat-token repeat-token repeat-token\n";
  kernel_note_metadata metadata{};
  kernel_write_disposition disposition{};
  expect_ok(kernel_write_note(
      handle,
      "repeat-api-note.md",
      content.data(),
      content.size(),
      nullptr,
      &metadata,
      &disposition));

  kernel_search_results results{};
  expect_ok(kernel_search_notes(handle, "repeat-token", &results));
  require_true(results.count == 1, "repeated term inside one note should return one API hit");
  require_true(std::string(results.hits[0].rel_path) == "repeat-api-note.md", "repeated-term API hit should preserve rel_path");
  kernel_free_search_results(&results);

  expect_ok(kernel_close(handle));
  std::filesystem::remove_all(vault);
  std::filesystem::remove_all(state_dir_for_vault(vault));
}

void test_search_api_matches_title_only_token() {
  const auto vault = make_temp_vault();
  kernel_handle* handle = nullptr;
  expect_ok(kernel_open_vault(vault.string().c_str(), &handle));

  const std::string content =
      "# TitleOnlyToken\n"
      "body text does not include the special title token\n";
  kernel_note_metadata metadata{};
  kernel_write_disposition disposition{};
  expect_ok(kernel_write_note(
      handle,
      "api-title-only.md",
      content.data(),
      content.size(),
      nullptr,
      &metadata,
      &disposition));

  kernel_search_results results{};
  expect_ok(kernel_search_notes(handle, "TitleOnlyToken", &results));
  require_true(results.count == 1, "title-only API query should match one note");
  require_true(std::string(results.hits[0].rel_path) == "api-title-only.md", "title-only API query should preserve rel_path");
  kernel_free_search_results(&results);

  expect_ok(kernel_close(handle));
  std::filesystem::remove_all(vault);
  std::filesystem::remove_all(state_dir_for_vault(vault));
}

void test_search_api_matches_filename_fallback_title_token() {
  const auto vault = make_temp_vault();
  kernel_handle* handle = nullptr;
  expect_ok(kernel_open_vault(vault.string().c_str(), &handle));

  const std::string content =
      "body text without heading\n";
  kernel_note_metadata metadata{};
  kernel_write_disposition disposition{};
  expect_ok(kernel_write_note(
      handle,
      "ApiFallbackTitleToken.md",
      content.data(),
      content.size(),
      nullptr,
      &metadata,
      &disposition));

  kernel_search_results results{};
  expect_ok(kernel_search_notes(handle, "ApiFallbackTitleToken", &results));
  require_true(results.count == 1, "filename-fallback title API query should match one note");
  require_true(std::string(results.hits[0].rel_path) == "ApiFallbackTitleToken.md", "filename-fallback title API query should preserve rel_path");
  kernel_free_search_results(&results);

  expect_ok(kernel_close(handle));
  std::filesystem::remove_all(vault);
  std::filesystem::remove_all(state_dir_for_vault(vault));
}

void test_search_api_reports_title_and_body_match_flags() {
  const auto vault = make_temp_vault();
  kernel_handle* handle = nullptr;
  expect_ok(kernel_open_vault(vault.string().c_str(), &handle));

  kernel_note_metadata metadata{};
  kernel_write_disposition disposition{};
  const std::string title_only =
      "# ApiTitleOnlyToken\n"
      "body text without the special title token\n";
  const std::string body_only =
      "# Generic API Title\n"
      "body contains ApiBodyOnlyToken\n";
  const std::string both =
      "# ApiBothToken\n"
      "body also contains ApiBothToken\n";

  expect_ok(kernel_write_note(handle, "api-title-flag.md", title_only.data(), title_only.size(), nullptr, &metadata, &disposition));
  expect_ok(kernel_write_note(handle, "api-body-flag.md", body_only.data(), body_only.size(), nullptr, &metadata, &disposition));
  expect_ok(kernel_write_note(handle, "api-both-flag.md", both.data(), both.size(), nullptr, &metadata, &disposition));

  kernel_search_results results{};
  expect_ok(kernel_search_notes(handle, "ApiTitleOnlyToken", &results));
  require_true(results.count == 1, "title-only API query should return one hit");
  require_true(
      results.hits[0].match_flags == KERNEL_SEARCH_MATCH_TITLE,
      "title-only API query should report TITLE match only");
  kernel_free_search_results(&results);

  expect_ok(kernel_search_notes(handle, "ApiBodyOnlyToken", &results));
  require_true(results.count == 1, "body-only API query should return one hit");
  require_true(
      results.hits[0].match_flags == KERNEL_SEARCH_MATCH_BODY,
      "body-only API query should report BODY match only");
  kernel_free_search_results(&results);

  expect_ok(kernel_search_notes(handle, "ApiBothToken", &results));
  require_true(results.count == 1, "shared API query should return one hit");
  require_true(
      results.hits[0].match_flags == (KERNEL_SEARCH_MATCH_TITLE | KERNEL_SEARCH_MATCH_BODY),
      "shared API query should report TITLE and BODY matches");
  kernel_free_search_results(&results);

  expect_ok(kernel_close(handle));
  std::filesystem::remove_all(vault);
  std::filesystem::remove_all(state_dir_for_vault(vault));
}

void test_search_api_limited_query_rejects_invalid_inputs_and_supports_limit() {
  const auto vault = make_temp_vault();
  kernel_handle* handle = nullptr;
  expect_ok(kernel_open_vault(vault.string().c_str(), &handle));

  kernel_note_metadata metadata{};
  kernel_write_disposition disposition{};
  expect_ok(kernel_write_note(handle, "b-limit-search.md", "# B\nlimit-search-token\n", 23, nullptr, &metadata, &disposition));
  expect_ok(kernel_write_note(handle, "a-limit-search.md", "# A\nlimit-search-token\n", 23, nullptr, &metadata, &disposition));

  kernel_search_results results{};
  require_true(
      kernel_search_notes_limited(handle, "", 1, &results).code == KERNEL_ERROR_INVALID_ARGUMENT,
      "empty limited search query should be invalid");
  require_true(
      kernel_search_notes_limited(handle, "limit-search-token", 0, &results).code == KERNEL_ERROR_INVALID_ARGUMENT,
      "zero-limit search query should be invalid");

  expect_ok(kernel_search_notes_limited(handle, "limit-search-token", 1, &results));
  require_true(results.count == 1, "limited search should cap result count");
  require_true(std::string(results.hits[0].rel_path) == "a-limit-search.md", "limited search should keep rel_path ordering");
  require_true(std::string(results.hits[0].title) == "A", "limited search should preserve hit titles");
  require_true(results.hits[0].match_flags == KERNEL_SEARCH_MATCH_BODY, "limited search should preserve BODY match flags");

  const kernel_status invalid_status = kernel_search_notes_limited(handle, "", 1, &results);
  const bool invalid_cleared = results.count == 0 && results.hits == nullptr;
  if (!invalid_cleared) {
    kernel_free_search_results(&results);
  }
  require_true(invalid_status.code == KERNEL_ERROR_INVALID_ARGUMENT, "invalid limited search query should still report INVALID_ARGUMENT");
  require_true(invalid_cleared, "invalid limited search query should clear stale output hits");

  expect_ok(kernel_close(handle));
  std::filesystem::remove_all(vault);
  std::filesystem::remove_all(state_dir_for_vault(vault));
}

void test_tag_query_returns_matching_notes_in_rel_path_order() {
  const auto vault = make_temp_vault();
  kernel_handle* handle = nullptr;
  expect_ok(kernel_open_vault(vault.string().c_str(), &handle));

  kernel_note_metadata metadata{};
  kernel_write_disposition disposition{};
  const std::string a_text = "# A\n#sharedtag\n";
  const std::string b_text = "# B\n#sharedtag\n";
  const std::string c_text = "# C\n#othertag\n";

  expect_ok(kernel_write_note(handle, "b-note.md", a_text.data(), a_text.size(), nullptr, &metadata, &disposition));
  expect_ok(kernel_write_note(handle, "a-note.md", b_text.data(), b_text.size(), nullptr, &metadata, &disposition));
  expect_ok(kernel_write_note(handle, "c-note.md", c_text.data(), c_text.size(), nullptr, &metadata, &disposition));

  kernel_search_results results{};
  expect_ok(kernel_query_tag_notes(handle, "sharedtag", static_cast<size_t>(-1), &results));
  require_true(results.count == 2, "tag query should return two matching notes");
  require_true(std::string(results.hits[0].rel_path) == "a-note.md", "tag query should order hits by rel_path ascending");
  require_true(std::string(results.hits[0].title) == "B", "tag query should preserve note titles");
  require_true(results.hits[0].match_flags == KERNEL_SEARCH_MATCH_NONE, "tag query hits should report no TITLE/BODY flags");
  require_true(std::string(results.hits[1].rel_path) == "b-note.md", "tag query should order hits by rel_path ascending");
  require_true(std::string(results.hits[1].title) == "A", "tag query should preserve note titles");
  require_true(results.hits[1].match_flags == KERNEL_SEARCH_MATCH_NONE, "tag query hits should report no TITLE/BODY flags");
  kernel_free_search_results(&results);

  expect_ok(kernel_close(handle));
  std::filesystem::remove_all(vault);
  std::filesystem::remove_all(state_dir_for_vault(vault));
}

void test_backlinks_query_returns_matching_sources() {
  const auto vault = make_temp_vault();
  kernel_handle* handle = nullptr;
  expect_ok(kernel_open_vault(vault.string().c_str(), &handle));

  kernel_note_metadata metadata{};
  kernel_write_disposition disposition{};
  const std::string target_text = "# Target Note\nbody\n";
  const std::string source_a_text = "# Source A\n[[Target Note]]\n";
  const std::string source_b_text = "# Source B\n[[Target Note]]\n";
  const std::string unrelated_text = "# Unrelated\n[[Other Note]]\n";

  expect_ok(kernel_write_note(handle, "target.md", target_text.data(), target_text.size(), nullptr, &metadata, &disposition));
  expect_ok(kernel_write_note(handle, "b-source.md", source_a_text.data(), source_a_text.size(), nullptr, &metadata, &disposition));
  expect_ok(kernel_write_note(handle, "a-source.md", source_b_text.data(), source_b_text.size(), nullptr, &metadata, &disposition));
  expect_ok(kernel_write_note(handle, "c-unrelated.md", unrelated_text.data(), unrelated_text.size(), nullptr, &metadata, &disposition));

  kernel_search_results results{};
  expect_ok(kernel_query_backlinks(handle, "target.md", static_cast<size_t>(-1), &results));
  require_true(results.count == 2, "backlinks query should return two matching source notes");
  require_true(std::string(results.hits[0].rel_path) == "a-source.md", "backlinks query should order hits by rel_path ascending");
  require_true(std::string(results.hits[0].title) == "Source B", "backlinks query should preserve source titles");
  require_true(results.hits[0].match_flags == KERNEL_SEARCH_MATCH_NONE, "backlinks query hits should report no TITLE/BODY flags");
  require_true(std::string(results.hits[1].rel_path) == "b-source.md", "backlinks query should order hits by rel_path ascending");
  require_true(std::string(results.hits[1].title) == "Source A", "backlinks query should preserve source titles");
  require_true(results.hits[1].match_flags == KERNEL_SEARCH_MATCH_NONE, "backlinks query hits should report no TITLE/BODY flags");
  kernel_free_search_results(&results);

  expect_ok(kernel_close(handle));
  std::filesystem::remove_all(vault);
  std::filesystem::remove_all(state_dir_for_vault(vault));
}

void test_backlinks_query_accepts_windows_style_relative_path() {
  const auto vault = make_temp_vault();
  kernel_handle* handle = nullptr;
  expect_ok(kernel_open_vault(vault.string().c_str(), &handle));

  kernel_note_metadata metadata{};
  kernel_write_disposition disposition{};
  const std::string target_text = "# Nested Target\nbody\n";
  const std::string source_text = "# Nested Source\n[[Nested Target]]\n";

  expect_ok(kernel_write_note(handle, "nested/target.md", target_text.data(), target_text.size(), nullptr, &metadata, &disposition));
  expect_ok(kernel_write_note(handle, "nested/source.md", source_text.data(), source_text.size(), nullptr, &metadata, &disposition));

  kernel_search_results results{};
  expect_ok(kernel_query_backlinks(handle, "nested/target.md", static_cast<size_t>(-1), &results));
  require_true(results.count == 1, "forward-slash backlinks query should find the nested source");
  require_true(std::string(results.hits[0].rel_path) == "nested/source.md", "forward-slash backlinks query should preserve source rel_path");

  expect_ok(kernel_query_backlinks(handle, "nested\\target.md", static_cast<size_t>(-1), &results));
  require_true(results.count == 1, "backlinks query should normalize Windows-style separators");
  require_true(std::string(results.hits[0].rel_path) == "nested/source.md", "normalized backlinks query should preserve source rel_path");
  require_true(std::string(results.hits[0].title) == "Nested Source", "normalized backlinks query should preserve source title");
  kernel_free_search_results(&results);

  expect_ok(kernel_close(handle));
  std::filesystem::remove_all(vault);
  std::filesystem::remove_all(state_dir_for_vault(vault));
}

void test_tag_and_backlinks_queries_follow_rewrite_recovery_and_rebuild() {
  const auto vault = make_temp_vault();
  const auto db_path = storage_db_for_vault(vault);
  kernel_handle* handle = nullptr;
  expect_ok(kernel_open_vault(vault.string().c_str(), &handle));

  kernel_note_metadata metadata{};
  kernel_write_disposition disposition{};
  const std::string target_text = "# Stable Target\nbody\n";
  const std::string original_source_text = "# Source\n#oldtag\n[[Stable Target]]\n";
  expect_ok(kernel_write_note(handle, "target.md", target_text.data(), target_text.size(), nullptr, &metadata, &disposition));
  expect_ok(kernel_write_note(handle, "source.md", original_source_text.data(), original_source_text.size(), nullptr, &metadata, &disposition));

  const std::string rewritten_source_text = "# Source\n#newtag\n[[Other Target]]\n";
  expect_ok(kernel_write_note(
      handle,
      "source.md",
      rewritten_source_text.data(),
      rewritten_source_text.size(),
      metadata.content_revision,
      &metadata,
      &disposition));

  kernel_search_results results{};
  expect_ok(kernel_query_tag_notes(handle, "oldtag", static_cast<size_t>(-1), &results));
  require_true(results.count == 0, "rewrite should remove old tag query hits");
  kernel_free_search_results(&results);
  expect_ok(kernel_query_backlinks(handle, "target.md", static_cast<size_t>(-1), &results));
  require_true(results.count == 0, "rewrite should remove stale backlinks query hits");
  kernel_free_search_results(&results);

  {
    std::lock_guard lock(handle->storage_mutex);
    exec_sql(handle->storage.connection, "DELETE FROM note_tags WHERE note_id=(SELECT note_id FROM notes WHERE rel_path='source.md');");
    exec_sql(handle->storage.connection, "INSERT INTO note_tags(note_id, tag) VALUES((SELECT note_id FROM notes WHERE rel_path='source.md'), 'staletag');");
    exec_sql(handle->storage.connection, "DELETE FROM note_links WHERE note_id=(SELECT note_id FROM notes WHERE rel_path='source.md');");
    exec_sql(handle->storage.connection, "INSERT INTO note_links(note_id, target) VALUES((SELECT note_id FROM notes WHERE rel_path='source.md'), 'Stable Target');");
  }

  expect_ok(kernel_rebuild_index(handle));

  expect_ok(kernel_query_tag_notes(handle, "staletag", static_cast<size_t>(-1), &results));
  require_true(results.count == 0, "rebuild should remove stale tag query hits");
  kernel_free_search_results(&results);
  expect_ok(kernel_query_tag_notes(handle, "newtag", static_cast<size_t>(-1), &results));
  require_true(results.count == 1, "rebuild should restore live tag query hits");
  kernel_free_search_results(&results);
  expect_ok(kernel_query_backlinks(handle, "target.md", static_cast<size_t>(-1), &results));
  require_true(results.count == 0, "rebuild should remove stale backlink query hits");
  kernel_free_search_results(&results);

  expect_ok(kernel_close(handle));

  prepare_state_dir_for_vault(vault);
  const auto journal_path = journal_path_for_vault(vault);
  const auto temp_path = vault / "source.md.recovery.tmp";
  const std::string recovered_source_text = "# Source\n#recovertag\n[[Stable Target]]\n";
  write_file_bytes(vault / "source.md", recovered_source_text);
  write_file_bytes(temp_path, "stale temp");
  require_true(
      !kernel::recovery::append_save_begin(
          journal_path,
          "tag-backlink-recovery-op",
          "source.md",
          temp_path),
      "recovery journal append should succeed");

  handle = nullptr;
  expect_ok(kernel_open_vault(vault.string().c_str(), &handle));
  require_index_ready(handle, "recovery-backed tag/backlink query test should settle to READY");

  expect_ok(kernel_query_tag_notes(handle, "recovertag", static_cast<size_t>(-1), &results));
  require_true(results.count == 1, "startup recovery should restore recovered tag query hits");
  kernel_free_search_results(&results);
  expect_ok(kernel_query_backlinks(handle, "target.md", static_cast<size_t>(-1), &results));
  require_true(results.count == 1, "startup recovery should restore recovered backlink query hits");
  require_true(std::string(results.hits[0].rel_path) == "source.md", "recovered backlink hit should preserve rel_path");
  kernel_free_search_results(&results);

  expect_ok(kernel_close(handle));
  std::filesystem::remove_all(vault);
  std::filesystem::remove_all(state_dir_for_vault(vault));
}

void test_tag_query_rejects_invalid_inputs_and_supports_limit() {
  const auto vault = make_temp_vault();
  kernel_handle* handle = nullptr;
  expect_ok(kernel_open_vault(vault.string().c_str(), &handle));

  kernel_note_metadata metadata{};
  kernel_write_disposition disposition{};
  expect_ok(kernel_write_note(handle, "b-note.md", "# B\n#limitag\n", 12, nullptr, &metadata, &disposition));
  expect_ok(kernel_write_note(handle, "a-note.md", "# A\n#limitag\n", 12, nullptr, &metadata, &disposition));

  kernel_search_results results{};
  require_true(
      kernel_query_tag_notes(handle, "", static_cast<size_t>(-1), &results).code == KERNEL_ERROR_INVALID_ARGUMENT,
      "empty tag query should be invalid");
  require_true(
      kernel_query_tag_notes(handle, "   \t", static_cast<size_t>(-1), &results).code == KERNEL_ERROR_INVALID_ARGUMENT,
      "whitespace-only tag query should be invalid");
  require_true(
      kernel_query_tag_notes(handle, "limitag", 0, &results).code == KERNEL_ERROR_INVALID_ARGUMENT,
      "zero-limit tag query should be invalid");

  expect_ok(kernel_query_tag_notes(handle, "limitag", 1, &results));
  require_true(results.count == 1, "tag query limit should cap results");
  require_true(std::string(results.hits[0].rel_path) == "a-note.md", "tag query limit should keep rel_path ordering");
  require_true(std::string(results.hits[0].title) == "A", "tag query limit should preserve hit titles");
  require_true(results.hits[0].match_flags == KERNEL_SEARCH_MATCH_NONE, "tag query hits should report no TITLE/BODY flags");

  const kernel_status invalid_status = kernel_query_tag_notes(handle, "", static_cast<size_t>(-1), &results);
  const bool invalid_cleared = results.count == 0 && results.hits == nullptr;
  if (!invalid_cleared) {
    kernel_free_search_results(&results);
  }
  require_true(invalid_status.code == KERNEL_ERROR_INVALID_ARGUMENT, "invalid tag query should still report INVALID_ARGUMENT");
  require_true(invalid_cleared, "invalid tag query should clear stale output hits");

  expect_ok(kernel_close(handle));
  std::filesystem::remove_all(vault);
  std::filesystem::remove_all(state_dir_for_vault(vault));
}

void test_backlinks_query_rejects_invalid_inputs_and_supports_limit() {
  const auto vault = make_temp_vault();
  kernel_handle* handle = nullptr;
  expect_ok(kernel_open_vault(vault.string().c_str(), &handle));

  kernel_note_metadata metadata{};
  kernel_write_disposition disposition{};
  const std::string target_text = "# Target Note\nbody\n";
  const std::string source_a_text = "# Source A\n[[Target Note]]\n";
  const std::string source_b_text = "# Source B\n[[Target Note]]\n";

  expect_ok(kernel_write_note(handle, "target.md", target_text.data(), target_text.size(), nullptr, &metadata, &disposition));
  expect_ok(kernel_write_note(handle, "b-source.md", source_a_text.data(), source_a_text.size(), nullptr, &metadata, &disposition));
  expect_ok(kernel_write_note(handle, "a-source.md", source_b_text.data(), source_b_text.size(), nullptr, &metadata, &disposition));

  kernel_search_results results{};
  require_true(
      kernel_query_backlinks(handle, "", static_cast<size_t>(-1), &results).code == KERNEL_ERROR_INVALID_ARGUMENT,
      "empty backlinks rel_path should be invalid");
  require_true(
      kernel_query_backlinks(handle, "..\\target.md", static_cast<size_t>(-1), &results).code == KERNEL_ERROR_INVALID_ARGUMENT,
      "path-traversal backlinks rel_path should be invalid");
  require_true(
      kernel_query_backlinks(handle, "target.md", 0, &results).code == KERNEL_ERROR_INVALID_ARGUMENT,
      "zero-limit backlinks query should be invalid");

  expect_ok(kernel_query_backlinks(handle, "target.md", 1, &results));
  require_true(results.count == 1, "backlinks query limit should cap results");
  require_true(std::string(results.hits[0].rel_path) == "a-source.md", "backlinks query limit should keep rel_path ordering");
  require_true(std::string(results.hits[0].title) == "Source B", "backlinks query limit should preserve source titles");
  require_true(results.hits[0].match_flags == KERNEL_SEARCH_MATCH_NONE, "backlinks query hits should report no TITLE/BODY flags");

  const kernel_status invalid_status = kernel_query_backlinks(handle, "", static_cast<size_t>(-1), &results);
  const bool invalid_cleared = results.count == 0 && results.hits == nullptr;
  if (!invalid_cleared) {
    kernel_free_search_results(&results);
  }
  require_true(invalid_status.code == KERNEL_ERROR_INVALID_ARGUMENT, "invalid backlinks query should still report INVALID_ARGUMENT");
  require_true(invalid_cleared, "invalid backlinks query should clear stale output hits");

  expect_ok(kernel_close(handle));
  std::filesystem::remove_all(vault);
  std::filesystem::remove_all(state_dir_for_vault(vault));
}

void test_background_watcher_indexes_external_create() {
  const auto vault = make_temp_vault();
  kernel_handle* handle = nullptr;
  expect_ok(kernel_open_vault(vault.string().c_str(), &handle));

  write_file_bytes(
      vault / "bg-create.md",
      "# Background Create\nbg-create-token\n");

  require_eventually(
      [&]() {
        kernel_search_results results{};
        const kernel_status status = kernel_search_notes(handle, "bg-create-token", &results);
        if (status.code != KERNEL_OK) {
          return false;
        }
        const bool matched =
            results.count == 1 &&
            std::string(results.hits[0].rel_path) == "bg-create.md";
        kernel_free_search_results(&results);
        return matched;
      },
      "background watcher should index external create");

  expect_ok(kernel_close(handle));
  std::filesystem::remove_all(vault);
  std::filesystem::remove_all(state_dir_for_vault(vault));
}

void test_background_watcher_updates_external_modify() {
  const auto vault = make_temp_vault();
  kernel_handle* handle = nullptr;
  expect_ok(kernel_open_vault(vault.string().c_str(), &handle));

  const std::string original =
      "# Background Modify\n"
      "bg-before-token\n";
  kernel_note_metadata metadata{};
  kernel_write_disposition disposition{};
  expect_ok(kernel_write_note(
      handle,
      "bg-modify.md",
      original.data(),
      original.size(),
      nullptr,
      &metadata,
      &disposition));

  write_file_bytes(
      vault / "bg-modify.md",
      "# Background Modify\n"
      "bg-after-token\n");

  require_eventually(
      [&]() {
        kernel_search_results old_results{};
        if (kernel_search_notes(handle, "bg-before-token", &old_results).code != KERNEL_OK) {
          return false;
        }
        const bool old_gone = old_results.count == 0;
        kernel_free_search_results(&old_results);

        kernel_search_results new_results{};
        if (kernel_search_notes(handle, "bg-after-token", &new_results).code != KERNEL_OK) {
          return false;
        }
        const bool new_present =
            new_results.count == 1 &&
            std::string(new_results.hits[0].rel_path) == "bg-modify.md";
        kernel_free_search_results(&new_results);
        return old_gone && new_present;
      },
      "background watcher should replace stale search rows after external modify");

  expect_ok(kernel_close(handle));
  std::filesystem::remove_all(vault);
  std::filesystem::remove_all(state_dir_for_vault(vault));
}

void test_background_watcher_applies_external_delete() {
  const auto vault = make_temp_vault();
  kernel_handle* handle = nullptr;
  expect_ok(kernel_open_vault(vault.string().c_str(), &handle));

  const std::string content =
      "# Background Delete\n"
      "bg-delete-token\n";
  kernel_note_metadata metadata{};
  kernel_write_disposition disposition{};
  expect_ok(kernel_write_note(
      handle,
      "bg-delete.md",
      content.data(),
      content.size(),
      nullptr,
      &metadata,
      &disposition));

  std::filesystem::remove(vault / "bg-delete.md");

  require_eventually(
      [&]() {
        kernel_search_results results{};
        if (kernel_search_notes(handle, "bg-delete-token", &results).code != KERNEL_OK) {
          return false;
        }
        const bool removed = results.count == 0;
        kernel_free_search_results(&results);
        if (!removed) {
          return false;
        }

        kernel_state_snapshot snapshot{};
        if (kernel_get_state(handle, &snapshot).code != KERNEL_OK) {
          return false;
        }
        return snapshot.indexed_note_count == 0;
      },
      "background watcher should remove deleted note from search and active count");

  expect_ok(kernel_close(handle));
  std::filesystem::remove_all(vault);
  std::filesystem::remove_all(state_dir_for_vault(vault));
}

void test_close_stops_background_watcher_until_reopen() {
  const auto vault = make_temp_vault();
  const auto db_path = storage_db_for_vault(vault);
  kernel_handle* handle = nullptr;
  expect_ok(kernel_open_vault(vault.string().c_str(), &handle));
  require_index_ready(handle, "close lifecycle test should start from a ready state");

  expect_ok(kernel_close(handle));

  const std::string rel_path = "closed-window.md";
  const std::string token = "closed-window-token";
  write_file_bytes(vault / rel_path, "# Closed Window\n" + token + "\n");
  std::this_thread::sleep_for(std::chrono::milliseconds(300));

  sqlite3* readonly_db = open_sqlite_readonly(storage_db_for_vault(vault));
  require_true(
      query_single_int(readonly_db, "SELECT COUNT(*) FROM notes WHERE is_deleted=0;") == 0,
      "closed kernel should not keep indexing external changes after kernel_close");
  sqlite3_close(readonly_db);

  handle = nullptr;
  expect_ok(kernel_open_vault(vault.string().c_str(), &handle));
  require_index_ready(handle, "reopen should catch up external changes that happened while closed");

  kernel_search_results results{};
  expect_ok(kernel_search_notes(handle, token.c_str(), &results));
  require_true(results.count == 1, "reopen should make closed-window external create searchable");
  require_true(std::string(results.hits[0].rel_path) == rel_path, "reopen should preserve rel_path after closed-window catch-up");
  kernel_free_search_results(&results);

  expect_ok(kernel_close(handle));
  std::filesystem::remove_all(vault);
  std::filesystem::remove_all(state_dir_for_vault(vault));
}

void test_close_releases_watcher_handles_so_vault_can_be_renamed() {
  const auto vault = make_temp_vault();
  const auto renamed_vault = vault.parent_path() / (vault.filename().string() + "-renamed");

  kernel_handle* handle = nullptr;
  expect_ok(kernel_open_vault(vault.string().c_str(), &handle));
  require_index_ready(handle, "close handle-release test should start from a ready state");

  expect_ok(kernel_close(handle));

  std::error_code rename_ec;
  std::filesystem::rename(vault, renamed_vault, rename_ec);
  require_true(!rename_ec, "kernel_close should release watcher handles so the vault directory can be renamed");

  std::filesystem::rename(renamed_vault, vault, rename_ec);
  require_true(!rename_ec, "renamed vault should be movable back after close");

  std::filesystem::remove_all(vault);
  std::filesystem::remove_all(state_dir_for_vault(vault));
}

void test_close_during_delayed_catch_up_does_not_commit_catch_up_results() {
  const auto vault = make_temp_vault();
  const auto db_path = storage_db_for_vault(vault);
  kernel::index::inject_full_rescan_delay_ms(500, 1);

  kernel_handle* handle = nullptr;
  expect_ok(kernel_open_vault(vault.string().c_str(), &handle));

  require_eventually(
      [&]() {
        kernel_state_snapshot snapshot{};
        return kernel_get_state(handle, &snapshot).code == KERNEL_OK &&
               snapshot.index_state == KERNEL_INDEX_CATCHING_UP;
      },
      "delayed catch-up should be observable before close");

  write_file_bytes(
      vault / "close-during-catchup.md",
      "# Close During Catch Up\nclose-during-catchup-token\n");

  expect_ok(kernel_close(handle));
  kernel::index::inject_full_rescan_delay_ms(0, 0);

  sqlite3* db = open_sqlite_readonly(db_path);
  require_true(
      query_single_int(db, "SELECT COUNT(*) FROM notes WHERE rel_path='close-during-catchup.md' AND is_deleted=0;") == 0,
      "closing during delayed catch-up should not commit catch-up results into sqlite");
  sqlite3_close(db);

  expect_ok(kernel_open_vault(vault.string().c_str(), &handle));
  require_eventually(
      [&]() {
        kernel_search_results results{};
        if (kernel_search_notes(handle, "close-during-catchup-token", &results).code != KERNEL_OK) {
          return false;
        }
        const bool indexed =
            results.count == 1 &&
            std::string(results.hits[0].rel_path) == "close-during-catchup.md";
        kernel_free_search_results(&results);
        if (!indexed) {
          return false;
        }

        kernel_state_snapshot snapshot{};
        return kernel_get_state(handle, &snapshot).code == KERNEL_OK &&
               snapshot.index_state == KERNEL_INDEX_READY &&
               snapshot.indexed_note_count == 1;
      },
      "reopen should reconcile the file through a fresh catch-up");

  expect_ok(kernel_close(handle));
  std::filesystem::remove_all(vault);
  std::filesystem::remove_all(state_dir_for_vault(vault));
}

void test_internal_write_suppression_does_not_swallow_later_external_modify() {
  const auto vault = make_temp_vault();
  kernel_handle* handle = nullptr;
  expect_ok(kernel_open_vault(vault.string().c_str(), &handle));
  require_index_ready(handle, "suppression test should start from a ready state");

  kernel_note_metadata metadata{};
  kernel_write_disposition disposition{};
  const std::string initial_text = "# Initial Title\ninitial-token\n";
  expect_ok(
      kernel_write_note(
          handle,
          "suppressed.md",
          initial_text.data(),
          initial_text.size(),
          nullptr,
          &metadata,
          &disposition));
  require_true(disposition == KERNEL_WRITE_WRITTEN, "initial internal write should persist note");

  const std::string external_text = "# External Title\nexternal-after-internal-token\n";
  write_file_bytes(vault / "suppressed.md", external_text);

  require_eventually(
      [&]() {
        kernel_search_results results{};
        const kernel_status status =
            kernel_search_notes(handle, "external-after-internal-token", &results);
        if (status.code != KERNEL_OK) {
          return false;
        }
        const bool matched =
            results.count == 1 &&
            std::string(results.hits[0].rel_path) == "suppressed.md";
        kernel_free_search_results(&results);
        return matched;
      },
      "external modify after internal write should not be swallowed by watcher suppression");

  kernel_search_results results{};
  expect_ok(kernel_search_notes(handle, "initial-token", &results));
  require_true(results.count == 0, "later external modify should replace the stale self-written FTS row");
  kernel_free_search_results(&results);

  expect_ok(kernel_close(handle));
  std::filesystem::remove_all(vault);
  std::filesystem::remove_all(state_dir_for_vault(vault));
}

void test_initial_catch_up_and_watcher_poll_do_not_double_apply_external_create() {
  const auto vault = make_temp_vault();
  const auto db_path = storage_db_for_vault(vault);

  kernel::index::inject_full_rescan_delay_ms(300, 1000);

  kernel_handle* handle = nullptr;
  expect_ok(kernel_open_vault(vault.string().c_str(), &handle));

  require_eventually(
      [&]() {
        kernel_state_snapshot snapshot{};
        return kernel_get_state(handle, &snapshot).code == KERNEL_OK &&
               snapshot.index_state == KERNEL_INDEX_CATCHING_UP;
      },
      "delayed initial catch-up should become observable before creating the external note");

  write_file_bytes(
      vault / "catchup-created.md",
      "# Catchup Create\ncatchup-create-token\n");

  require_index_ready(handle, "delayed initial catch-up should eventually settle back to READY");
  kernel::index::inject_full_rescan_delay_ms(0, 0);

  kernel_search_results results{};
  expect_ok(kernel_search_notes(handle, "catchup-create-token", &results));
  require_true(results.count == 1, "create during initial catch-up should become searchable exactly once");
  require_true(std::string(results.hits[0].rel_path) == "catchup-created.md", "catch-up create hit should preserve rel_path");
  kernel_free_search_results(&results);

  kernel_state_snapshot snapshot{};
  expect_ok(kernel_get_state(handle, &snapshot));
  require_true(snapshot.indexed_note_count == 1, "catch-up plus later watcher poll should not double-count the created note");

  sqlite3* db = open_sqlite_readonly(db_path);
  require_true(
      query_single_int(db, "SELECT COUNT(*) FROM notes WHERE is_deleted=0 AND rel_path='catchup-created.md';") == 1,
      "catch-up plus later watcher poll should leave exactly one live notes row");
  sqlite3_close(db);

  expect_ok(kernel_close(handle));
  std::filesystem::remove_all(vault);
  std::filesystem::remove_all(state_dir_for_vault(vault));
}

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

void test_rebuild_reconciles_attachment_missing_state() {
  const auto vault = make_temp_vault();
  std::filesystem::create_directories(vault / "assets");
  write_file_bytes(vault / "assets" / "rebuild-attachment.png", "attachment-bytes");

  kernel_handle* handle = nullptr;
  expect_ok(kernel_open_vault(vault.string().c_str(), &handle));
  require_index_ready(handle, "attachment rebuild test should start from a ready state");

  const std::string content =
      "# Rebuild Attachment\n"
      "![Figure](assets/rebuild-attachment.png)\n";
  kernel_note_metadata metadata{};
  kernel_write_disposition disposition{};
  expect_ok(kernel_write_note(
      handle,
      "rebuild-attachment.md",
      content.data(),
      content.size(),
      nullptr,
      &metadata,
      &disposition));

  std::filesystem::remove(vault / "assets" / "rebuild-attachment.png");

  {
    std::lock_guard lock(handle->storage_mutex);
    exec_sql(
        handle->storage.connection,
        "UPDATE attachments SET is_missing=0 WHERE rel_path='assets/rebuild-attachment.png';");
  }

  expect_ok(kernel_rebuild_index(handle));

  sqlite3* readonly_db = open_sqlite_readonly(storage_db_for_vault(vault));
  require_true(
      query_single_int(
          readonly_db,
          "SELECT COUNT(*) FROM attachments "
          "WHERE rel_path='assets/rebuild-attachment.png' AND is_missing=1;") == 1,
      "rebuild should mark missing attachment paths from disk truth");
  require_true(
      query_single_int(
          readonly_db,
          "SELECT COUNT(*) FROM note_attachment_refs "
          "WHERE note_id=(SELECT note_id FROM notes WHERE rel_path='rebuild-attachment.md') "
          "AND attachment_rel_path='assets/rebuild-attachment.png';") == 1,
      "rebuild should preserve note attachment refs while refreshing missing state");
  sqlite3_close(readonly_db);

  require_single_note_attachment_ref_state(
      handle,
      "rebuild-attachment.md",
      "assets/rebuild-attachment.png",
      KERNEL_ATTACHMENT_PRESENCE_MISSING,
      1,
      KERNEL_ATTACHMENT_KIND_IMAGE_LIKE,
      true,
      "rebuild should preserve the live attachment ref through the formal public surface");
  require_attachment_lookup_state(
      handle,
      "assets/rebuild-attachment.png",
      KERNEL_ATTACHMENT_PRESENCE_MISSING,
      1,
      KERNEL_ATTACHMENT_KIND_IMAGE_LIKE,
      true,
      "rebuild should refresh missing attachment metadata through the formal public surface");

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

void test_export_diagnostics_writes_json_snapshot() {
  const auto vault = make_temp_vault();
  const auto db_path = storage_db_for_vault(vault);
  const auto export_path = make_temp_export_path("diagnostics.json");
  std::filesystem::create_directories(vault / "assets");
  write_file_bytes(vault / "assets" / "diag-present.png", "diag-present-bytes");
  kernel_handle* handle = nullptr;
  expect_ok(kernel_open_vault(vault.string().c_str(), &handle));
  require_index_ready(handle, "diagnostics snapshot should export from a settled ready state");

  const std::string content =
      "# Diagnostics Title\n"
      "diagnostics-token\n";
  kernel_note_metadata metadata{};
  kernel_write_disposition disposition{};
  expect_ok(kernel_write_note(
      handle,
      "diag.md",
      content.data(),
      content.size(),
      nullptr,
      &metadata,
      &disposition));
  const std::string attachment_content =
      "# Diagnostics Attachments\n"
      "![Present](assets/diag-present.png)\n"
      "![Missing](assets/diag-missing.png)\n";
  expect_ok(kernel_write_note(
      handle,
      "diag-attachments.md",
      attachment_content.data(),
      attachment_content.size(),
      nullptr,
      &metadata,
      &disposition));
  {
    std::lock_guard lock(handle->storage_mutex);
    exec_sql(
        handle->storage.connection,
        "INSERT INTO attachments(rel_path, file_size, mtime_ns, is_missing) "
        "VALUES('assets/diag-orphan.bin', 17, 23, 0);");
  }

  expect_ok(kernel_export_diagnostics(handle, export_path.string().c_str()));

  const AttachmentAnomalySnapshot anomaly_snapshot =
      read_attachment_anomaly_snapshot(db_path);
  require_true(
      anomaly_snapshot.missing_attachment_count == 1,
      "diagnostics snapshot should seed exactly one missing live attachment");
  require_true(
      anomaly_snapshot.orphaned_attachment_count >= 1,
      "diagnostics orphan coverage should seed at least one orphaned attachment row");

  const std::string exported = read_file_text(export_path);
  require_true(exported.find("\"session_state\":\"OPEN\"") != std::string::npos, "diagnostics should include session state");
  require_true(exported.find("\"index_state\":\"READY\"") != std::string::npos, "diagnostics should include index state");
  require_true(exported.find("\"index_fault_reason\":\"\"") != std::string::npos, "healthy diagnostics should clear index fault reason");
  require_true(exported.find("\"index_fault_code\":0") != std::string::npos, "healthy diagnostics should clear index fault code");
  require_true(exported.find("\"index_fault_at_ns\":0") != std::string::npos, "healthy diagnostics should clear index fault timestamp");
  require_true(
      exported.find("\"last_recovery_outcome\":\"clean_startup\"") != std::string::npos,
      "healthy diagnostics should report clean_startup as the last recovery outcome");
  require_true(
      exported.find("\"last_recovery_detected_corrupt_tail\":false") != std::string::npos,
      "healthy diagnostics should report no corrupt recovery tail on a clean startup");
  require_true(
      exported.find("\"last_recovery_at_ns\":") != std::string::npos,
      "healthy diagnostics should export last_recovery_at_ns");
  require_true(
      exported.find("\"last_recovery_at_ns\":0") == std::string::npos,
      "healthy diagnostics should export a non-zero last_recovery_at_ns after startup");
  require_true(exported.find("\"indexed_note_count\":2") != std::string::npos, "diagnostics should include indexed note count");
  require_true(exported.find("\"attachment_count\":2") != std::string::npos, "diagnostics should include attachment count");
  require_true(exported.find("\"attachment_live_count\":2") != std::string::npos, "diagnostics should include live attachment count");
  require_true(
      exported.find("\"missing_attachment_count\":" + std::to_string(anomaly_snapshot.missing_attachment_count)) !=
          std::string::npos,
      "diagnostics should include missing attachment count that matches SQLite truth");
  require_true(
      exported.find("\"orphaned_attachment_count\":" + std::to_string(anomaly_snapshot.orphaned_attachment_count)) !=
          std::string::npos,
      "diagnostics should include orphaned attachment count that matches SQLite truth");
  require_true(
      exported.find("\"missing_attachment_paths\":[\"assets/diag-missing.png\"]") != std::string::npos,
      "diagnostics should export a stable missing attachment path summary");
  require_true(
      exported.find("\"orphaned_attachment_paths\":[\"assets/diag-orphan.bin\"]") != std::string::npos,
      "diagnostics should export a stable orphaned attachment path summary");
  require_true(
      exported.find("\"attachment_anomaly_path_summary_limit\":16") != std::string::npos,
      "diagnostics should export the frozen attachment anomaly path summary limit");
  require_true(
      exported.find("\"attachment_anomaly_summary\":\"" + anomaly_snapshot.summary + "\"") !=
          std::string::npos,
      "diagnostics should summarize attachment anomalies from SQLite truth");
  require_true(
      exported.find("\"last_attachment_recount_reason\":\"\"") != std::string::npos,
      "healthy diagnostics should leave last_attachment_recount_reason empty before any rebuild or watcher full rescan");
  require_true(
      exported.find("\"last_attachment_recount_at_ns\":0") != std::string::npos,
      "healthy diagnostics should leave last_attachment_recount_at_ns zero before any rebuild or watcher full rescan");
  require_true(
      exported.find("\"attachment_public_surface_revision\":\"track2_batch1_public_surface_v1\"") != std::string::npos,
      "diagnostics should expose the current attachment public surface revision");
  require_true(
      exported.find("\"attachment_metadata_contract_revision\":\"track2_batch2_metadata_contract_v1\"") !=
          std::string::npos,
      "diagnostics should expose the current attachment metadata contract revision");
  require_true(
      exported.find("\"attachment_kind_mapping_revision\":\"track2_batch1_extension_mapping_v1\"") != std::string::npos,
      "diagnostics should expose the current attachment kind mapping revision");
  require_true(
      exported.find("\"logger_backend\":\"null_logger\"") != std::string::npos,
      "diagnostics should expose the current logger backend");
  require_true(
      exported.find("\"search_contract_revision\":\"track1_batch4_ranking_v1\"") != std::string::npos,
      "diagnostics should expose the current search contract revision");
  require_true(
      exported.find("\"search_backend\":\"sqlite_fts5\"") != std::string::npos,
      "diagnostics should expose the current search backend");
  require_true(
      exported.find("\"search_snippet_mode\":\"body_single_segment_plaintext_fixed_length\"") != std::string::npos,
      "diagnostics should expose the current snippet mode");
  require_true(
      exported.find("\"search_snippet_max_bytes\":160") != std::string::npos,
      "diagnostics should expose the current snippet length cap");
  require_true(
      exported.find("\"search_pagination_mode\":\"offset_limit_exact_total_v1\"") != std::string::npos,
      "diagnostics should expose the current pagination mode");
  require_true(
      exported.find("\"search_filters_mode\":\"kind_tag_path_prefix_v1\"") != std::string::npos,
      "diagnostics should expose the current filters mode");
  require_true(
      exported.find("\"search_ranking_mode\":\"fts_title_tag_v1\"") != std::string::npos,
      "diagnostics should expose the current ranking mode");
  require_true(
      exported.find("\"search_supported_kinds\":\"note,attachment,all\"") != std::string::npos,
      "diagnostics should expose the frozen supported search kinds");
  require_true(
      exported.find("\"search_supported_filters\":\"kind,tag,path_prefix\"") != std::string::npos,
      "diagnostics should expose the frozen supported search filters");
  require_true(
      exported.find("\"search_ranking_supported_kinds\":\"note,all_note_branch\"") != std::string::npos,
      "diagnostics should expose the supported Ranking v1 kinds");
  require_true(
      exported.find("\"search_ranking_tie_break\":\"rel_path_asc\"") != std::string::npos,
      "diagnostics should expose the frozen Ranking v1 tie-break");
  require_true(
      exported.find("\"search_page_max_limit\":128") != std::string::npos,
      "diagnostics should expose the frozen search page max limit");
  require_true(
      exported.find("\"search_total_hits_supported\":true") != std::string::npos,
      "diagnostics should expose that exact total hit counts are supported");
  require_true(
      exported.find("\"search_include_deleted_supported\":false") != std::string::npos,
      "diagnostics should expose that include_deleted remains disabled");
  require_true(
      exported.find("\"search_attachment_path_only\":true") != std::string::npos,
      "diagnostics should expose that attachment search is path-only");
  require_true(
      exported.find("\"search_title_hit_boost_enabled\":true") != std::string::npos,
      "diagnostics should expose that Ranking v1 title-hit boosting is enabled");
  require_true(
      exported.find("\"search_tag_exact_boost_enabled\":true") != std::string::npos,
      "diagnostics should expose that Ranking v1 tag-exact boosting is enabled");
  require_true(
      exported.find("\"search_tag_exact_boost_single_token_only\":true") != std::string::npos,
      "diagnostics should expose the frozen single-token boundary for tag-exact boosting");
  require_true(
      exported.find("\"search_all_kind_order\":\"notes_then_attachments_rel_path_asc\"") != std::string::npos,
      "diagnostics should expose the frozen kind=all ordering");
  require_true(
      exported.find("\"last_continuity_fallback_reason\":\"\"") != std::string::npos,
      "healthy diagnostics should export an empty continuity fallback reason");
  require_true(
      exported.find("\"last_continuity_fallback_at_ns\":0") != std::string::npos,
      "healthy diagnostics should export a zero continuity fallback timestamp before any fallback occurs");
  require_true(exported.find("\"pending_recovery_ops\":0") != std::string::npos, "diagnostics should include pending recovery count");
  require_true(exported.find("\"vault_root\":") != std::string::npos, "diagnostics should include vault root");
  require_true(exported.find("\"state_dir\":") != std::string::npos, "diagnostics should include state dir");
  require_true(exported.find("\"storage_db_path\":") != std::string::npos, "diagnostics should include storage db path");
  require_true(exported.find("\"recovery_journal_path\":") != std::string::npos, "diagnostics should include recovery journal path");

  expect_ok(kernel_close(handle));
  std::filesystem::remove_all(export_path.parent_path());
  std::filesystem::remove_all(vault);
  std::filesystem::remove_all(state_dir_for_vault(vault));
}

void test_export_diagnostics_reports_recent_events_in_runtime_order() {
  const auto vault = make_temp_vault();
  const auto export_path = vault / "diagnostics-recent-events.json";
  kernel_handle* handle = nullptr;
  expect_ok(kernel_open_vault(vault.string().c_str(), &handle));
  require_index_ready(handle, "recent-events diagnostics test should start from a ready state");

  kernel::core::record_continuity_fallback(handle, "rename_old_without_new");
  kernel::index::inject_full_rescan_delay_ms(300, 1);
  expect_ok(kernel_rebuild_index(handle));
  kernel::index::inject_full_rescan_delay_ms(0, 0);

  expect_ok(kernel_export_diagnostics(handle, export_path.string().c_str()));
  const std::string exported = read_file_text(export_path);
  require_true(
      exported.find("\"recent_events\":[") != std::string::npos,
      "diagnostics should export a recent_events array");

  const std::size_t recovery_pos = exported.find("\"kind\":\"startup_recovery\"");
  const std::size_t continuity_pos = exported.find("\"kind\":\"continuity_fallback\"");
  const std::size_t rebuild_started_pos = exported.find("\"kind\":\"rebuild_started\"");
  const std::size_t rebuild_succeeded_pos = exported.find("\"kind\":\"rebuild_succeeded\"");
  require_true(
      recovery_pos != std::string::npos &&
          continuity_pos != std::string::npos &&
          rebuild_started_pos != std::string::npos &&
          rebuild_succeeded_pos != std::string::npos,
      "diagnostics recent_events should include startup recovery, continuity fallback, rebuild_started, and rebuild_succeeded");
  require_true(
      recovery_pos < continuity_pos &&
          continuity_pos < rebuild_started_pos &&
          rebuild_started_pos < rebuild_succeeded_pos,
      "diagnostics recent_events should retain stable append order");

  expect_ok(kernel_close(handle));
  std::filesystem::remove_all(vault);
  std::filesystem::remove_all(state_dir_for_vault(vault));
}

void test_export_diagnostics_reports_last_continuity_fallback_reason() {
  const auto vault = make_temp_vault();
  const auto export_path = vault / "diagnostics-continuity-fallback.json";
  kernel_handle* handle = nullptr;
  expect_ok(kernel_open_vault(vault.string().c_str(), &handle));
  require_index_ready(handle, "continuity fallback diagnostics test should start from a ready state");

  kernel::core::record_continuity_fallback(handle, "rename_old_without_new");

  expect_ok(kernel_export_diagnostics(handle, export_path.string().c_str()));
  const std::string exported = read_file_text(export_path);
  require_true(
      exported.find("\"last_continuity_fallback_reason\":\"rename_old_without_new\"") != std::string::npos,
      "diagnostics should export the latest continuity fallback reason");
  require_true(
      exported.find("\"last_continuity_fallback_at_ns\":0") == std::string::npos,
      "diagnostics should export a non-zero continuity fallback timestamp after a fallback is recorded");

  expect_ok(kernel_close(handle));
  std::filesystem::remove_all(vault);
  std::filesystem::remove_all(state_dir_for_vault(vault));
}

void test_export_diagnostics_reports_recovery_outcome_after_corrupt_tail_cleanup() {
  const auto vault = make_temp_vault();
  const auto state_dir = state_dir_for_vault(vault);
  const auto journal_path = journal_path_for_vault(vault);
  const auto export_path = vault / "diagnostics-recovery-outcome.json";
  const auto target_path = vault / "diag-recover.md";
  const auto temp_path = target_path.parent_path() / "diag-recover.md.codex-recovery.tmp";

  prepare_state_dir_for_vault(vault);
  write_file_bytes(
      target_path,
      "# Recovery Outcome\n"
      "recovery-outcome-token\n");
  write_file_bytes(temp_path, "stale-temp");

  require_true(
      kernel::recovery::append_save_begin(
          journal_path,
          "diag-recovery-op",
          "diag-recover.md",
          temp_path)
          .value() == 0,
      "manual SAVE_BEGIN append should succeed before corrupt-tail diagnostics test");
  append_crc_mismatch_tail_record(journal_path);

  kernel_handle* handle = nullptr;
  expect_ok(kernel_open_vault(vault.string().c_str(), &handle));
  require_index_ready(handle, "recovery-outcome diagnostics test should settle to READY after startup recovery");

  expect_ok(kernel_export_diagnostics(handle, export_path.string().c_str()));
  const std::string exported = read_file_text(export_path);
  require_true(
      exported.find("\"last_recovery_outcome\":\"recovered_pending_saves\"") != std::string::npos,
      "diagnostics should report recovered_pending_saves after replaying startup recovery work");
  require_true(
      exported.find("\"last_recovery_detected_corrupt_tail\":true") != std::string::npos,
      "diagnostics should report that startup recovery detected and ignored a corrupt tail");
  require_true(
      exported.find("\"last_recovery_at_ns\":") != std::string::npos,
      "diagnostics should export last_recovery_at_ns after startup recovery work");
  require_true(
      exported.find("\"last_recovery_at_ns\":0") == std::string::npos,
      "diagnostics should export a non-zero last_recovery_at_ns after replaying startup recovery work");
  require_true(
      exported.find("\"pending_recovery_ops\":0") != std::string::npos,
      "diagnostics should still report zero pending recovery ops after recovery cleanup");

  expect_ok(kernel_close(handle));
  std::filesystem::remove_all(vault);
  std::filesystem::remove_all(state_dir);
}

void test_export_diagnostics_reports_temp_only_recovery_cleanup() {
  const auto vault = make_temp_vault();
  const auto state_dir = state_dir_for_vault(vault);
  const auto journal_path = journal_path_for_vault(vault);
  const auto export_path = vault / "diagnostics-temp-only-recovery.json";
  const auto temp_path = vault / "temp-only-recovery.tmp";

  prepare_state_dir_for_vault(vault);
  write_file_bytes(temp_path, "orphan-temp");

  require_true(
      kernel::recovery::append_save_begin(
          journal_path,
          "temp-only-diag-op",
          "missing.md",
          temp_path)
          .value() == 0,
      "temp-only SAVE_BEGIN append should succeed before diagnostics recovery-outcome test");

  kernel_handle* handle = nullptr;
  expect_ok(kernel_open_vault(vault.string().c_str(), &handle));
  require_index_ready(handle, "temp-only diagnostics recovery test should settle to READY after cleanup");

  expect_ok(kernel_export_diagnostics(handle, export_path.string().c_str()));
  const std::string exported = read_file_text(export_path);
  require_true(
      exported.find("\"last_recovery_outcome\":\"cleaned_temp_only_pending_saves\"") != std::string::npos,
      "diagnostics should distinguish temp-only recovery cleanup from note recovery");
  require_true(
      exported.find("\"last_recovery_detected_corrupt_tail\":false") != std::string::npos,
      "temp-only recovery cleanup should not imply a corrupt journal tail");
  require_true(
      exported.find("\"last_recovery_at_ns\":") != std::string::npos,
      "diagnostics should export last_recovery_at_ns after temp-only startup cleanup");
  require_true(
      exported.find("\"last_recovery_at_ns\":0") == std::string::npos,
      "diagnostics should export a non-zero last_recovery_at_ns after temp-only startup cleanup");

  expect_ok(kernel_close(handle));
  std::filesystem::remove_all(vault);
  std::filesystem::remove_all(state_dir);
}

void test_export_diagnostics_reflects_catching_up_without_fault_and_without_stale_count() {
  const auto vault = make_temp_vault();
  const auto export_path = vault / "diagnostics-catching-up.json";
  kernel_handle* handle = nullptr;
  expect_ok(kernel_open_vault(vault.string().c_str(), &handle));

  const std::string original =
      "# Catching Up Export Title\n"
      "catching-up-export-token\n";
  kernel_note_metadata metadata{};
  kernel_write_disposition disposition{};
  expect_ok(kernel_write_note(
      handle,
      "catching-up-export.md",
      original.data(),
      original.size(),
      nullptr,
      &metadata,
      &disposition));
  require_index_ready(handle, "seed write should settle before close");
  expect_ok(kernel_close(handle));

  std::filesystem::remove(vault / "catching-up-export.md");
  kernel::index::inject_full_rescan_delay_ms(300, 1);

  expect_ok(kernel_open_vault(vault.string().c_str(), &handle));
  require_eventually(
      [&]() {
        kernel_state_snapshot snapshot{};
        return kernel_get_state(handle, &snapshot).code == KERNEL_OK &&
               snapshot.index_state == KERNEL_INDEX_CATCHING_UP;
      },
      "delayed reopen should expose CATCHING_UP before diagnostics export");

  expect_ok(kernel_export_diagnostics(handle, export_path.string().c_str()));
  const std::string exported = read_file_text(export_path);
  require_true(
      exported.find("\"index_state\":\"CATCHING_UP\"") != std::string::npos,
      "diagnostics should export CATCHING_UP during initial reconciliation");
  require_true(
      exported.find("\"index_fault_reason\":\"\"") != std::string::npos,
      "diagnostics should keep fault reason empty during healthy catch-up");
  require_true(
      exported.find("\"index_fault_code\":0") != std::string::npos,
      "diagnostics should keep fault code zero during healthy catch-up");
  require_true(
      exported.find("\"index_fault_at_ns\":0") != std::string::npos,
      "diagnostics should keep fault timestamp zero during healthy catch-up");
  require_true(
      exported.find("\"indexed_note_count\":0") != std::string::npos,
      "diagnostics should not export a stale indexed note count during CATCHING_UP");

  require_index_ready(handle, "catch-up export test should eventually settle back to READY");

  kernel::index::inject_full_rescan_delay_ms(0, 0);
  expect_ok(kernel_close(handle));
  std::filesystem::remove_all(vault);
  std::filesystem::remove_all(state_dir_for_vault(vault));
}

void test_export_diagnostics_reflects_watcher_runtime_fault_state() {
  const auto vault = make_temp_vault();
  const auto export_path = vault / "diagnostics-fault.json";
  kernel_handle* handle = nullptr;
  expect_ok(kernel_open_vault(vault.string().c_str(), &handle));

  kernel::watcher::inject_next_poll_error(handle->watcher_session, std::errc::io_error);
  require_eventually(
      [&]() {
        kernel_state_snapshot snapshot{};
        return kernel_get_state(handle, &snapshot).code == KERNEL_OK &&
               snapshot.index_state == KERNEL_INDEX_UNAVAILABLE;
      },
      "watcher fault should degrade state before diagnostics export");

  expect_ok(kernel_export_diagnostics(handle, export_path.string().c_str()));
  const std::string exported = read_file_text(export_path);
  require_true(exported.find("\"index_state\":\"UNAVAILABLE\"") != std::string::npos, "diagnostics should reflect degraded watcher state");
  require_true(exported.find("\"index_fault_reason\":\"watcher_poll_failed\"") != std::string::npos, "diagnostics should expose watcher poll failure reason");
  require_true(
      exported.find("\"index_fault_code\":" + std::to_string(std::make_error_code(std::errc::io_error).value())) != std::string::npos,
      "diagnostics should expose watcher poll failure code");
  require_true(
      exported.find("\"index_fault_at_ns\":0") == std::string::npos,
      "diagnostics should expose a non-zero watcher poll failure timestamp");

  expect_ok(kernel_close(handle));
  std::filesystem::remove_all(vault);
  std::filesystem::remove_all(state_dir_for_vault(vault));
}

void test_export_diagnostics_reflects_rebuilding_without_fault() {
  const auto vault = make_temp_vault();
  const auto export_path = vault / "diagnostics-rebuilding.json";
  kernel_handle* handle = nullptr;
  expect_ok(kernel_open_vault(vault.string().c_str(), &handle));
  require_index_ready(handle, "rebuild diagnostics test should start from a ready state");

  kernel::index::inject_full_rescan_delay_ms(300, 1000);

  kernel_status rebuild_status{KERNEL_ERROR_INTERNAL};
  std::jthread rebuild_thread([&]() {
    rebuild_status = kernel_rebuild_index(handle);
  });

  require_eventually(
      [&]() {
        if (kernel_export_diagnostics(handle, export_path.string().c_str()).code != KERNEL_OK) {
          return false;
        }
        const std::string exported = read_file_text(export_path);
        return exported.find("\"index_state\":\"REBUILDING\"") != std::string::npos &&
               exported.find("\"index_fault_reason\":\"\"") != std::string::npos &&
               exported.find("\"index_fault_code\":0") != std::string::npos &&
               exported.find("\"index_fault_at_ns\":0") != std::string::npos;
      },
      "diagnostics should expose REBUILDING without fault fields during delayed rebuild");

  rebuild_thread.join();
  kernel::index::inject_full_rescan_delay_ms(0, 0);
  expect_ok(rebuild_status);

  expect_ok(kernel_close(handle));
  std::filesystem::remove_all(vault);
  std::filesystem::remove_all(state_dir_for_vault(vault));
}

void test_export_diagnostics_reflects_initial_catch_up_fault_state() {
  const auto vault = make_temp_vault();
  const auto export_path = vault / "diagnostics-catch-up-fault.json";
  kernel::index::inject_full_rescan_failures(std::errc::io_error, 1000);

  kernel_handle* handle = nullptr;
  expect_ok(kernel_open_vault(vault.string().c_str(), &handle));

  require_eventually(
      [&]() {
        kernel_state_snapshot snapshot{};
        return kernel_get_state(handle, &snapshot).code == KERNEL_OK &&
               snapshot.index_state == KERNEL_INDEX_UNAVAILABLE;
      },
      "initial catch-up fault should degrade state before diagnostics export");

  expect_ok(kernel_export_diagnostics(handle, export_path.string().c_str()));
  const std::string exported = read_file_text(export_path);
  require_true(exported.find("\"index_state\":\"UNAVAILABLE\"") != std::string::npos, "diagnostics should reflect degraded catch-up state");
  require_true(exported.find("\"index_fault_reason\":\"initial_catch_up_failed\"") != std::string::npos, "diagnostics should expose initial catch-up failure reason");
  require_true(
      exported.find("\"index_fault_code\":" + std::to_string(std::make_error_code(std::errc::io_error).value())) != std::string::npos,
      "diagnostics should expose initial catch-up failure code");
  require_true(
      exported.find("\"index_fault_at_ns\":0") == std::string::npos,
      "diagnostics should expose a non-zero initial catch-up failure timestamp");

  kernel::index::inject_full_rescan_failures(std::errc::io_error, 0);
  expect_ok(kernel_close(handle));
  std::filesystem::remove_all(vault);
  std::filesystem::remove_all(state_dir_for_vault(vault));
}

void test_rebuild_failure_sets_unavailable_and_exports_fault() {
  const auto vault = make_temp_vault();
  const auto export_path = vault / "diagnostics-rebuild-fault.json";
  kernel_handle* handle = nullptr;
  expect_ok(kernel_open_vault(vault.string().c_str(), &handle));
  require_index_ready(handle, "rebuild fault test should start from a ready state");

  kernel::index::inject_full_rescan_failures(std::errc::io_error, 1);
  const kernel_status rebuild_status = kernel_rebuild_index(handle);
  require_true(rebuild_status.code == KERNEL_ERROR_IO, "rebuild failure should surface as KERNEL_ERROR_IO");

  kernel_state_snapshot snapshot{};
  expect_ok(kernel_get_state(handle, &snapshot));
  require_true(snapshot.index_state == KERNEL_INDEX_UNAVAILABLE, "failed rebuild should leave index_state UNAVAILABLE");

  expect_ok(kernel_export_diagnostics(handle, export_path.string().c_str()));
  const std::string exported = read_file_text(export_path);
  require_true(exported.find("\"index_state\":\"UNAVAILABLE\"") != std::string::npos, "rebuild failure diagnostics should reflect degraded index state");
  require_true(exported.find("\"index_fault_reason\":\"rebuild_failed\"") != std::string::npos, "rebuild failure diagnostics should expose rebuild_failed");
  require_true(
      exported.find("\"index_fault_code\":" + std::to_string(std::make_error_code(std::errc::io_error).value())) != std::string::npos,
      "rebuild failure diagnostics should expose rebuild error code");
  require_true(
      exported.find("\"index_fault_at_ns\":0") == std::string::npos,
      "rebuild failure diagnostics should expose a non-zero failure timestamp");

  expect_ok(kernel_close(handle));
  std::filesystem::remove_all(vault);
  std::filesystem::remove_all(state_dir_for_vault(vault));
}

void test_rebuild_recovers_ready_state_and_clears_fault_fields() {
  const auto vault = make_temp_vault();
  const auto export_path = vault / "diagnostics-rebuild-recovered.json";
  kernel_handle* handle = nullptr;
  expect_ok(kernel_open_vault(vault.string().c_str(), &handle));
  require_index_ready(handle, "rebuild recovery test should start from a ready state");

  kernel::index::inject_full_rescan_failures(std::errc::io_error, 1);
  const kernel_status failed_rebuild_status = kernel_rebuild_index(handle);
  require_true(failed_rebuild_status.code == KERNEL_ERROR_IO, "seed rebuild failure should fail with KERNEL_ERROR_IO");

  expect_ok(kernel_rebuild_index(handle));

  kernel_state_snapshot snapshot{};
  expect_ok(kernel_get_state(handle, &snapshot));
  require_true(snapshot.index_state == KERNEL_INDEX_READY, "successful rebuild retry should restore READY");

  expect_ok(kernel_export_diagnostics(handle, export_path.string().c_str()));
  const std::string exported = read_file_text(export_path);
  require_true(exported.find("\"index_state\":\"READY\"") != std::string::npos, "recovered rebuild diagnostics should export READY state");
  require_true(exported.find("\"index_fault_reason\":\"\"") != std::string::npos, "successful rebuild retry should clear fault reason");
  require_true(exported.find("\"index_fault_code\":0") != std::string::npos, "successful rebuild retry should clear fault code");
  require_true(exported.find("\"index_fault_at_ns\":0") != std::string::npos, "successful rebuild retry should clear fault timestamp");

  expect_ok(kernel_close(handle));
  std::filesystem::remove_all(vault);
  std::filesystem::remove_all(state_dir_for_vault(vault));
}

void test_export_diagnostics_reports_last_rebuild_result_and_timestamp() {
  const auto vault = make_temp_vault();
  const auto db_path = storage_db_for_vault(vault);
  const auto export_path = make_temp_export_path("diagnostics-last-rebuild.json");
  std::filesystem::create_directories(vault / "assets");
  write_file_bytes(vault / "assets" / "rebuild-diag.png", "rebuild-diag-bytes");
  const std::string content =
      "# Rebuild Diagnostics Attachment\n"
      "![Figure](assets/rebuild-diag.png)\n";
  write_file_bytes(vault / "rebuild-diagnostics-attachment.md", content);
  kernel_handle* handle = nullptr;
  require_true(
      kernel_open_vault(vault.string().c_str(), &handle).code == KERNEL_OK,
      "last rebuild diagnostics test should open vault");
  require_index_ready(handle, "last rebuild diagnostics test should start from a ready state");

  require_true(
      kernel_rebuild_index(handle).code == KERNEL_OK,
      "last rebuild diagnostics test should complete a successful rebuild");

  require_true(
      kernel_export_diagnostics(handle, export_path.string().c_str()).code == KERNEL_OK,
      "last rebuild diagnostics test should export diagnostics");
  const AttachmentAnomalySnapshot anomaly_snapshot =
      read_attachment_anomaly_snapshot(db_path);
  require_true(
      anomaly_snapshot.missing_attachment_count == 0,
      "successful rebuild diagnostics should leave no missing live attachments");
  require_true(
      anomaly_snapshot.orphaned_attachment_count == 0,
      "successful rebuild diagnostics should leave no orphaned attachment rows");
  const std::string exported = read_file_text(export_path);
  require_true(
      exported.find("\"last_rebuild_result\":\"succeeded\"") != std::string::npos,
      "diagnostics should export the latest successful rebuild result");
  require_true(
      exported.find("\"has_last_rebuild_result\":true") != std::string::npos,
      "diagnostics should report has_last_rebuild_result=true after a successful rebuild");
  require_true(
      exported.find("\"last_rebuild_result_code\":0") != std::string::npos,
      "diagnostics should export the latest successful rebuild result code");
  require_true(
      exported.find("\"last_rebuild_at_ns\":0") == std::string::npos,
      "diagnostics should export a non-zero rebuild timestamp after success");
  require_true(
      exported.find("\"last_rebuild_duration_ms\":") != std::string::npos,
      "diagnostics should export last_rebuild_duration_ms after success");
  require_true(
      exported.find("\"rebuild_current_generation\":0") != std::string::npos,
      "diagnostics should clear the current rebuild generation after rebuild completion");
  require_true(
      exported.find("\"rebuild_last_completed_generation\":1") != std::string::npos,
      "diagnostics should export the latest completed rebuild generation after one successful rebuild");
  require_true(
      exported.find("\"rebuild_current_started_at_ns\":0") != std::string::npos,
      "diagnostics should clear rebuild_current_started_at_ns once no rebuild is in flight");
  require_true(
      exported.find("\"missing_attachment_count\":" + std::to_string(anomaly_snapshot.missing_attachment_count)) !=
          std::string::npos,
      "successful rebuild diagnostics should export missing attachment count that matches SQLite truth");
  require_true(
      exported.find("\"orphaned_attachment_count\":" + std::to_string(anomaly_snapshot.orphaned_attachment_count)) !=
          std::string::npos,
      "successful rebuild diagnostics should export orphaned attachment count that matches SQLite truth");
  require_true(
      exported.find("\"missing_attachment_paths\":[]") != std::string::npos,
      "successful rebuild diagnostics should export an empty missing attachment path summary");
  require_true(
      exported.find("\"orphaned_attachment_paths\":[]") != std::string::npos,
      "successful rebuild diagnostics should export an empty orphaned attachment path summary");
  require_true(
      exported.find("\"attachment_anomaly_summary\":\"" + anomaly_snapshot.summary + "\"") !=
          std::string::npos,
      "successful rebuild diagnostics should summarize attachment anomalies from the exported counts");
  require_true(
      exported.find("\"last_attachment_recount_reason\":\"rebuild\"") != std::string::npos,
      "successful rebuild diagnostics should report attachment recount reason as rebuild");
  require_true(
      exported.find("\"last_attachment_recount_at_ns\":0") == std::string::npos,
      "successful rebuild diagnostics should export a non-zero attachment recount timestamp");

  require_true(
      kernel_close(handle).code == KERNEL_OK,
      "last rebuild diagnostics test should close vault");
  std::filesystem::remove_all(export_path.parent_path());
  std::filesystem::remove_all(vault);
  std::filesystem::remove_all(state_dir_for_vault(vault));
}

void test_export_diagnostics_reports_last_attachment_recount_after_watcher_full_rescan() {
  const auto vault = make_temp_vault();
  const auto db_path = storage_db_for_vault(vault);
  const auto export_path = make_temp_export_path("diagnostics-watcher-full-rescan.json");
  std::filesystem::create_directories(vault / "assets");
  std::filesystem::create_directories(vault / "docs");
  write_file_bytes(vault / "assets" / "overflow-live.png", "overflow-live-bytes");
  write_file_bytes(vault / "assets" / "overflow-stale.png", "overflow-stale-bytes");
  const std::string before =
      "# Watcher Full Rescan Attachment\n"
      "![Live](assets/overflow-live.png)\n"
      "![Stale](assets/overflow-stale.png)\n";
  write_file_bytes(vault / "watcher-full-rescan-attachment.md", before);

  kernel_handle* handle = nullptr;
  require_true(
      kernel_open_vault(vault.string().c_str(), &handle).code == KERNEL_OK,
      "watcher full-rescan diagnostics test should open vault");
  require_index_ready(handle, "watcher full-rescan diagnostics test should start from a ready state");

  std::filesystem::remove(vault / "assets" / "overflow-stale.png");
  write_file_bytes(vault / "docs" / "overflow-fresh.pdf", "overflow-fresh-bytes");
  const std::string after =
      "# Watcher Full Rescan Attachment\n"
      "![Live](assets/overflow-live.png)\n"
      "![Stale](assets/overflow-stale.png)\n"
      "[Fresh](docs/overflow-fresh.pdf)\n";
  write_file_bytes(vault / "watcher-full-rescan-attachment.md", after);

  kernel::watcher::inject_next_poll_overflow(handle->watcher_session);
  require_eventually(
      [&]() {
        kernel_attachment_list refs{};
        const kernel_status refs_status = kernel_query_note_attachment_refs(
            handle,
            "watcher-full-rescan-attachment.md",
            static_cast<size_t>(-1),
            &refs);
        if (refs_status.code != KERNEL_OK) {
          return false;
        }

        const bool refs_match =
            refs.count == 3 &&
            std::string(refs.attachments[0].rel_path) == "assets/overflow-live.png" &&
            refs.attachments[0].presence == KERNEL_ATTACHMENT_PRESENCE_PRESENT &&
            std::string(refs.attachments[1].rel_path) == "assets/overflow-stale.png" &&
            refs.attachments[1].presence == KERNEL_ATTACHMENT_PRESENCE_MISSING &&
            std::string(refs.attachments[2].rel_path) == "docs/overflow-fresh.pdf" &&
            refs.attachments[2].presence == KERNEL_ATTACHMENT_PRESENCE_PRESENT;
        kernel_free_attachment_list(&refs);

        std::lock_guard runtime_lock(handle->runtime_mutex);
        return refs_match &&
               handle->runtime.last_attachment_recount.reason == "watcher_full_rescan" &&
               handle->runtime.last_attachment_recount.at_ns != 0;
      },
      "watcher full-rescan diagnostics test should observe attachment recount after overflow-driven rescan");

  require_true(
      kernel_export_diagnostics(handle, export_path.string().c_str()).code == KERNEL_OK,
      "watcher full-rescan diagnostics test should export diagnostics");
  const AttachmentAnomalySnapshot anomaly_snapshot =
      read_attachment_anomaly_snapshot(db_path);
  require_true(
      anomaly_snapshot.missing_attachment_count == 1,
      "watcher full-rescan diagnostics should keep exactly one missing live attachment in the seeded scenario");
  require_true(
      anomaly_snapshot.orphaned_attachment_count == 0,
      "watcher full-rescan diagnostics should avoid orphaned attachment rows when exporting outside the watched vault");
  const std::string exported = read_file_text(export_path);
  require_true(
      exported.find("\"last_attachment_recount_reason\":\"watcher_full_rescan\"") != std::string::npos,
      "watcher full-rescan diagnostics should report watcher_full_rescan as the last attachment recount reason");
  require_true(
      exported.find("\"last_attachment_recount_at_ns\":0") == std::string::npos,
      "watcher full-rescan diagnostics should export a non-zero attachment recount timestamp");
  require_true(
      exported.find("\"attachment_anomaly_summary\":\"" + anomaly_snapshot.summary + "\"") !=
          std::string::npos,
      "watcher full-rescan diagnostics should summarize attachment anomalies from SQLite truth");
  require_true(
      exported.find("\"missing_attachment_count\":" + std::to_string(anomaly_snapshot.missing_attachment_count)) !=
          std::string::npos,
      "watcher full-rescan diagnostics should still export refreshed missing attachment count");
  require_true(
      exported.find("\"orphaned_attachment_count\":" + std::to_string(anomaly_snapshot.orphaned_attachment_count)) !=
          std::string::npos,
      "watcher full-rescan diagnostics should report the orphaned attachment count from SQLite truth");
  require_true(
      exported.find("\"missing_attachment_paths\":[\"assets/overflow-stale.png\"]") !=
          std::string::npos,
      "watcher full-rescan diagnostics should export the refreshed missing attachment path summary");
  require_true(
      exported.find("\"orphaned_attachment_paths\":[]") != std::string::npos,
      "watcher full-rescan diagnostics should export an empty orphaned attachment path summary");

  require_true(
      kernel_close(handle).code == KERNEL_OK,
      "watcher full-rescan diagnostics test should close vault");
  std::filesystem::remove_all(export_path.parent_path());
  std::filesystem::remove_all(vault);
  std::filesystem::remove_all(state_dir_for_vault(vault));
}

void test_export_diagnostics_reports_last_rebuild_duration_after_delayed_failure() {
  const auto vault = make_temp_vault();
  const auto export_path = vault / "diagnostics-last-rebuild-failure-duration.json";
  kernel_handle* handle = nullptr;
  expect_ok(kernel_open_vault(vault.string().c_str(), &handle));
  require_index_ready(handle, "failed rebuild duration diagnostics test should start from a ready state");

  kernel::index::inject_full_rescan_delay_ms(300, 1);
  kernel::index::inject_full_rescan_failures(std::errc::io_error, 1);
  const kernel_status rebuild_status = kernel_rebuild_index(handle);
  require_true(
      rebuild_status.code == KERNEL_ERROR_IO,
      "failed rebuild duration diagnostics test should fail with KERNEL_ERROR_IO");
  kernel::index::inject_full_rescan_delay_ms(0, 0);

  expect_ok(kernel_export_diagnostics(handle, export_path.string().c_str()));
  const std::string exported = read_file_text(export_path);
  require_true(
      exported.find("\"last_rebuild_result\":\"failed\"") != std::string::npos,
      "diagnostics should export the latest failed rebuild result");
  require_true(
      exported.find("\"last_rebuild_duration_ms\":0") == std::string::npos,
      "diagnostics should export a non-zero rebuild duration after a delayed rebuild failure");

  expect_ok(kernel_close(handle));
  std::filesystem::remove_all(vault);
  std::filesystem::remove_all(state_dir_for_vault(vault));
}

void test_export_diagnostics_preserves_last_rebuild_result_code_while_next_task_runs() {
  const auto vault = make_temp_vault();
  const auto export_path = vault / "diagnostics-last-rebuild-running.json";
  kernel_handle* handle = nullptr;
  expect_ok(kernel_open_vault(vault.string().c_str(), &handle));
  require_index_ready(handle, "running rebuild diagnostics code test should start from a ready state");

  expect_ok(kernel_start_rebuild_index(handle));
  expect_ok(kernel_wait_for_rebuild(handle, 5000));

  kernel::index::inject_full_rescan_delay_ms(300, 1000);
  expect_ok(kernel_start_rebuild_index(handle));

  require_eventually(
      [&]() {
        if (kernel_export_diagnostics(handle, export_path.string().c_str()).code != KERNEL_OK) {
          return false;
        }
        const std::string exported = read_file_text(export_path);
        return exported.find("\"rebuild_in_flight\":true") != std::string::npos &&
               exported.find("\"has_last_rebuild_result\":true") != std::string::npos &&
               exported.find("\"last_rebuild_result\":\"succeeded\"") != std::string::npos &&
               exported.find("\"last_rebuild_result_code\":0") != std::string::npos &&
               exported.find("\"rebuild_current_generation\":2") != std::string::npos &&
               exported.find("\"rebuild_last_completed_generation\":1") != std::string::npos &&
               exported.find("\"rebuild_current_started_at_ns\":0") == std::string::npos;
      },
      "diagnostics should preserve the last completed rebuild result code and expose a non-zero rebuild_current_started_at_ns while the next rebuild task is already in flight");

  expect_ok(kernel_wait_for_rebuild(handle, 5000));
  kernel::index::inject_full_rescan_delay_ms(0, 0);

  expect_ok(kernel_close(handle));
  std::filesystem::remove_all(vault);
  std::filesystem::remove_all(state_dir_for_vault(vault));
}

void test_export_diagnostics_retains_fault_history_after_recovery() {
  const auto vault = make_temp_vault();
  const auto export_path = vault / "diagnostics-fault-history.json";
  kernel_handle* handle = nullptr;
  expect_ok(kernel_open_vault(vault.string().c_str(), &handle));
  require_index_ready(handle, "fault history diagnostics test should start from a ready state");

  kernel::index::inject_full_rescan_failures(std::errc::io_error, 1);
  const kernel_status failed_rebuild_status = kernel_rebuild_index(handle);
  require_true(failed_rebuild_status.code == KERNEL_ERROR_IO, "seed rebuild failure should fail with KERNEL_ERROR_IO");

  expect_ok(kernel_rebuild_index(handle));
  expect_ok(kernel_export_diagnostics(handle, export_path.string().c_str()));
  const std::string exported = read_file_text(export_path);
  require_true(
      exported.find("\"index_fault_reason\":\"\"") != std::string::npos,
      "fault history diagnostics should clear the live fault after recovery");
  require_true(
      exported.find("\"index_fault_history\":[") != std::string::npos,
      "fault history diagnostics should export a fault history array");
  require_true(
      exported.find("\"reason\":\"rebuild_failed\"") != std::string::npos,
      "fault history diagnostics should retain the previous rebuild failure");
  require_true(
      exported.find("\"last_rebuild_result\":\"succeeded\"") != std::string::npos,
      "fault history diagnostics should reflect the latest successful rebuild result");

  expect_ok(kernel_close(handle));
  std::filesystem::remove_all(vault);
  std::filesystem::remove_all(state_dir_for_vault(vault));
}

void test_background_watcher_error_sets_index_unavailable() {
  const auto vault = make_temp_vault();
  kernel_handle* handle = nullptr;
  expect_ok(kernel_open_vault(vault.string().c_str(), &handle));

  kernel::watcher::inject_next_poll_error(handle->watcher_session, std::errc::io_error);

  require_eventually(
      [&]() {
        kernel_state_snapshot snapshot{};
        if (kernel_get_state(handle, &snapshot).code != KERNEL_OK) {
          return false;
        }
        return snapshot.index_state == KERNEL_INDEX_UNAVAILABLE;
      },
      "background watcher error should downgrade index_state to UNAVAILABLE");

  expect_ok(kernel_close(handle));
  std::filesystem::remove_all(vault);
  std::filesystem::remove_all(state_dir_for_vault(vault));
}

void test_background_watcher_recovers_index_state_after_later_success() {
  const auto vault = make_temp_vault();
  kernel_handle* handle = nullptr;
  expect_ok(kernel_open_vault(vault.string().c_str(), &handle));

  kernel::watcher::inject_next_poll_error(handle->watcher_session, std::errc::io_error);

  require_eventually(
      [&]() {
        kernel_state_snapshot snapshot{};
        if (kernel_get_state(handle, &snapshot).code != KERNEL_OK) {
          return false;
        }
        return snapshot.index_state == KERNEL_INDEX_UNAVAILABLE;
      },
      "background watcher error should first downgrade index_state");

  write_file_bytes(
      vault / "bg-recover.md",
      "# Background Recover\nbg-recover-token\n");

  require_eventually(
      [&]() {
        kernel_search_results results{};
        if (kernel_search_notes(handle, "bg-recover-token", &results).code != KERNEL_OK) {
          return false;
        }
        const bool indexed =
            results.count == 1 &&
            std::string(results.hits[0].rel_path) == "bg-recover.md";
        kernel_free_search_results(&results);
        if (!indexed) {
          return false;
        }

        kernel_state_snapshot snapshot{};
        if (kernel_get_state(handle, &snapshot).code != KERNEL_OK) {
          return false;
        }
        return snapshot.index_state == KERNEL_INDEX_READY;
      },
      "background watcher should recover index_state to READY after later successful polling");

  expect_ok(kernel_close(handle));
  std::filesystem::remove_all(vault);
  std::filesystem::remove_all(state_dir_for_vault(vault));
}

void test_background_watcher_recovers_to_ready_without_external_work_and_clears_live_fault() {
  const auto vault = make_temp_vault();
  const auto export_path = vault / "diagnostics-watcher-recovered.json";
  kernel_handle* handle = nullptr;
  expect_ok(kernel_open_vault(vault.string().c_str(), &handle));

  kernel::watcher::inject_next_poll_error(handle->watcher_session, std::errc::io_error);

  require_eventually(
      [&]() {
        kernel_state_snapshot snapshot{};
        return kernel_get_state(handle, &snapshot).code == KERNEL_OK &&
               snapshot.index_state == KERNEL_INDEX_UNAVAILABLE;
      },
      "transient watcher poll fault should first degrade index_state to UNAVAILABLE");

  require_eventually(
      [&]() {
        kernel_state_snapshot snapshot{};
        return kernel_get_state(handle, &snapshot).code == KERNEL_OK &&
               snapshot.index_state == KERNEL_INDEX_READY;
      },
      "watcher should automatically recover to READY after a later healthy poll without external work");

  expect_ok(kernel_export_diagnostics(handle, export_path.string().c_str()));
  const std::string exported = read_file_text(export_path);
  require_true(
      exported.find("\"index_state\":\"READY\"") != std::string::npos,
      "diagnostics should export READY after automatic watcher recovery");
  require_true(
      exported.find("\"index_fault_reason\":\"\"") != std::string::npos,
      "diagnostics should clear the live watcher fault after automatic recovery");
  require_true(
      exported.find("\"index_fault_code\":0") != std::string::npos,
      "diagnostics should clear the live watcher fault code after automatic recovery");
  require_true(
      exported.find("\"index_fault_at_ns\":0") != std::string::npos,
      "diagnostics should clear the live watcher fault timestamp after automatic recovery");
  require_true(
      exported.find("\"reason\":\"watcher_poll_failed\"") != std::string::npos,
      "diagnostics should retain watcher poll failures in fault history after automatic recovery");

  expect_ok(kernel_close(handle));
  std::filesystem::remove_all(vault);
  std::filesystem::remove_all(state_dir_for_vault(vault));
}

void test_repeated_watcher_poll_faults_do_not_duplicate_fault_history_before_recovery() {
  const auto vault = make_temp_vault();
  const auto export_path = vault / "diagnostics-watcher-fault-dedup.json";
  kernel_handle* handle = nullptr;
  expect_ok(kernel_open_vault(vault.string().c_str(), &handle));

  kernel::watcher::inject_next_poll_errors(handle->watcher_session, std::errc::io_error, 3);

  require_eventually(
      [&]() {
        kernel_state_snapshot snapshot{};
        return kernel_get_state(handle, &snapshot).code == KERNEL_OK &&
               snapshot.index_state == KERNEL_INDEX_UNAVAILABLE;
      },
      "repeated watcher poll faults should first degrade index_state to UNAVAILABLE");

  require_eventually(
      [&]() {
        kernel_state_snapshot snapshot{};
        return kernel_get_state(handle, &snapshot).code == KERNEL_OK &&
               snapshot.index_state == KERNEL_INDEX_READY;
      },
      "watcher should automatically recover to READY after repeated transient poll faults");

  expect_ok(kernel_export_diagnostics(handle, export_path.string().c_str()));
  const std::string exported = read_file_text(export_path);
  require_true(
      count_occurrences(exported, "\"reason\":\"watcher_poll_failed\"") == 1,
      "repeated identical watcher poll faults should collapse to one history record before recovery");

  expect_ok(kernel_close(handle));
  std::filesystem::remove_all(vault);
  std::filesystem::remove_all(state_dir_for_vault(vault));
}

void test_repeated_watcher_poll_faults_back_off_before_auto_recovery() {
  const auto vault = make_temp_vault();
  const auto export_path = vault / "diagnostics-watcher-fault-backoff.json";
  kernel_handle* handle = nullptr;
  expect_ok(kernel_open_vault(vault.string().c_str(), &handle));

  const auto injected_at = std::chrono::steady_clock::now();
  kernel::watcher::inject_next_poll_errors(handle->watcher_session, std::errc::io_error, 3);

  require_eventually(
      [&]() {
        kernel_state_snapshot snapshot{};
        return kernel_get_state(handle, &snapshot).code == KERNEL_OK &&
               snapshot.index_state == KERNEL_INDEX_UNAVAILABLE;
      },
      "repeated watcher poll faults should first degrade index_state to UNAVAILABLE");

  std::this_thread::sleep_for(std::chrono::milliseconds(40));

  kernel_state_snapshot snapshot{};
  expect_ok(kernel_get_state(handle, &snapshot));
  require_true(
      snapshot.index_state == KERNEL_INDEX_UNAVAILABLE,
      "repeated watcher poll faults should stay UNAVAILABLE for at least one backoff window before auto-recovery");

  expect_ok(kernel_export_diagnostics(handle, export_path.string().c_str()));
  const std::string exported = read_file_text(export_path);
  require_true(
      exported.find("\"index_fault_reason\":\"watcher_poll_failed\"") != std::string::npos,
      "diagnostics should keep the live watcher poll fault visible during backoff");

  std::chrono::steady_clock::time_point recovered_at{};
  require_eventually(
      [&]() {
        kernel_state_snapshot recovered{};
        const bool ready =
            kernel_get_state(handle, &recovered).code == KERNEL_OK &&
            recovered.index_state == KERNEL_INDEX_READY;
        if (ready) {
          recovered_at = std::chrono::steady_clock::now();
        }
        return ready;
      },
      "watcher should eventually auto-recover after the throttled retry window");
  require_true(
      std::chrono::duration_cast<std::chrono::milliseconds>(recovered_at - injected_at).count() >= 180,
      "repeated watcher poll faults should incur a retry backoff before READY is restored");

  expect_ok(kernel_close(handle));
  std::filesystem::remove_all(vault);
  std::filesystem::remove_all(state_dir_for_vault(vault));
}

void test_close_interrupts_watcher_fault_backoff_promptly() {
  const auto vault = make_temp_vault();
  kernel_handle* handle = nullptr;
  expect_ok(kernel_open_vault(vault.string().c_str(), &handle));

  kernel::watcher::inject_next_poll_errors(handle->watcher_session, std::errc::io_error, 20);

  require_eventually(
      [&]() {
        kernel_state_snapshot snapshot{};
        return kernel_get_state(handle, &snapshot).code == KERNEL_OK &&
               snapshot.index_state == KERNEL_INDEX_UNAVAILABLE;
      },
      "repeated watcher poll faults should first degrade index_state before close-backoff regression");

  const auto close_started_at = std::chrono::steady_clock::now();
  expect_ok(kernel_close(handle));
  const auto close_elapsed_ms =
      std::chrono::duration_cast<std::chrono::milliseconds>(
          std::chrono::steady_clock::now() - close_started_at)
          .count();

  require_true(
      close_elapsed_ms < 150,
      "kernel_close should interrupt watcher fault backoff promptly instead of waiting through retry sleeps");

  std::filesystem::remove_all(vault);
  std::filesystem::remove_all(state_dir_for_vault(vault));
}

void test_initial_catch_up_failure_degrades_and_then_recovers_index_state() {
  const auto vault = make_temp_vault();
  kernel::index::inject_full_rescan_failures(std::errc::io_error, 1000);

  kernel_handle* handle = nullptr;
  expect_ok(kernel_open_vault(vault.string().c_str(), &handle));

  require_eventually(
      [&]() {
        kernel_state_snapshot snapshot{};
        return kernel_get_state(handle, &snapshot).code == KERNEL_OK &&
               snapshot.index_state == KERNEL_INDEX_UNAVAILABLE;
      },
      "initial catch-up failure should degrade index_state to UNAVAILABLE");

  kernel::index::inject_full_rescan_failures(std::errc::io_error, 0);

  require_eventually(
      [&]() {
        kernel_state_snapshot snapshot{};
        return kernel_get_state(handle, &snapshot).code == KERNEL_OK &&
               snapshot.index_state == KERNEL_INDEX_READY;
      },
      "initial catch-up should recover to READY after later successful rescan");

  expect_ok(kernel_close(handle));
  std::filesystem::remove_all(vault);
  std::filesystem::remove_all(state_dir_for_vault(vault));
}

void test_phase1_alpha_smoke_flow() {
  const auto vault = make_temp_vault();
  const auto export_path = vault / "alpha-smoke-diagnostics.json";
  kernel_handle* handle = nullptr;
  expect_ok(kernel_open_vault(vault.string().c_str(), &handle));
  require_index_ready(handle, "alpha smoke should start from a ready index state");

  const std::string initial_content =
      "# Alpha Smoke Title\n"
      "alpha-smoke-before-token\n"
      "#alphasmoke\n"
      "[[AlphaSmokeLink]]\n";
  kernel_note_metadata metadata{};
  kernel_write_disposition disposition{};
  expect_ok(kernel_write_note(
      handle,
      "alpha-smoke.md",
      initial_content.data(),
      initial_content.size(),
      nullptr,
      &metadata,
      &disposition));
  require_true(disposition == KERNEL_WRITE_WRITTEN, "alpha smoke write should persist note content");

  kernel_search_results results{};
  expect_ok(kernel_search_notes(handle, "alpha-smoke-before-token", &results));
  require_true(results.count == 1, "alpha smoke initial search should find the written note");
  require_true(std::string(results.hits[0].rel_path) == "alpha-smoke.md", "alpha smoke initial search should preserve rel_path");
  kernel_free_search_results(&results);

  write_file_bytes(
      vault / "alpha-smoke.md",
      "# Alpha Smoke Title Updated\n"
      "alpha-smoke-after-token\n"
      "#alphasmokeupdated\n"
      "[[AlphaSmokeLinkUpdated]]\n");

  require_eventually(
      [&]() {
        kernel_search_results old_results{};
        if (kernel_search_notes(handle, "alpha-smoke-before-token", &old_results).code != KERNEL_OK) {
          return false;
        }
        const bool old_gone = old_results.count == 0;
        kernel_free_search_results(&old_results);

        kernel_search_results new_results{};
        if (kernel_search_notes(handle, "alpha-smoke-after-token", &new_results).code != KERNEL_OK) {
          return false;
        }
        const bool new_present =
            new_results.count == 1 &&
            std::string(new_results.hits[0].rel_path) == "alpha-smoke.md";
        kernel_free_search_results(&new_results);

        return old_gone && new_present;
      },
      "alpha smoke watcher path should reconcile external modify");

  expect_ok(kernel_rebuild_index(handle));

  kernel_state_snapshot snapshot{};
  expect_ok(kernel_get_state(handle, &snapshot));
  require_true(snapshot.index_state == KERNEL_INDEX_READY, "alpha smoke rebuild should leave the index ready");
  require_true(snapshot.indexed_note_count == 1, "alpha smoke rebuild should preserve one active note");

  expect_ok(kernel_export_diagnostics(handle, export_path.string().c_str()));
  const std::string exported = read_file_text(export_path);
  require_true(exported.find("\"index_state\":\"READY\"") != std::string::npos, "alpha smoke diagnostics should export READY state");
  require_true(exported.find("\"index_fault_reason\":\"\"") != std::string::npos, "alpha smoke diagnostics should export a cleared fault reason");
  require_true(exported.find("\"indexed_note_count\":1") != std::string::npos, "alpha smoke diagnostics should export the active note count");

  expect_ok(kernel_close(handle));
  std::filesystem::remove_all(vault);
  std::filesystem::remove_all(state_dir_for_vault(vault));
}

}  // namespace

int main() {
  try {
    test_open_and_state_layers();
    test_index_state_reports_catching_up_during_delayed_initial_scan();
    test_open_vault_catches_up_external_modify_while_closed();
    test_catching_up_hides_stale_indexed_count_until_reconciled();
    test_open_creates_state_dir_and_storage_db();
    test_write_and_read_roundtrip();
    test_write_appends_save_begin_and_commit_to_journal();
    test_get_state_reflects_persisted_notes_and_recovery_queue();
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
    test_open_vault_reopen_preserves_schema_v1();
    test_same_content_write_is_noop();
    test_external_edit_causes_conflict();
    test_empty_content_note_is_allowed();
    test_write_requires_output_pointers();
    test_write_persists_parser_derived_tags_and_wikilinks();
    test_rewrite_replaces_old_parser_derived_rows();
    test_write_persists_attachment_metadata_and_refs();
    test_rewrite_replaces_old_attachment_refs();
    test_attachment_api_lists_note_refs_in_parser_order();
    test_attachment_api_reports_missing_state_and_rejects_invalid_inputs();
    test_attachment_public_surface_lists_live_catalog_and_single_attachment();
    test_attachment_public_surface_metadata_contract_covers_kind_mapping_and_missing_carry_forward();
    test_attachment_public_surface_note_refs_and_referrers_are_stable();
    test_attachment_public_surface_excludes_orphaned_paths_and_matches_search();
    test_attachment_api_rewrite_recovery_and_rebuild_follow_live_state();
    test_attachment_api_observes_attachment_rename_reconciliation();
    test_attachment_full_rescan_reconciles_mixed_changes_through_formal_public_surface();
  test_startup_recovery_replaces_stale_parser_derived_rows();
  test_startup_recovery_replaces_stale_attachment_refs_and_metadata();
  test_startup_recovery_plus_reopen_catch_up_removes_deleted_note_drift();
  test_reopen_catch_up_repairs_attachment_missing_state_after_closed_window_delete();
  test_close_during_watcher_fault_backoff_leaves_delete_for_reopen_catch_up();
  test_close_during_watcher_fault_backoff_leaves_modify_for_reopen_catch_up();
  test_close_during_watcher_fault_backoff_leaves_create_for_reopen_catch_up();
  test_close_during_watcher_fault_backoff_leaves_attachment_create_for_reopen_catch_up();
  test_close_during_watcher_fault_backoff_leaves_attachment_delete_for_reopen_catch_up();
  test_close_during_watcher_fault_backoff_leaves_attachment_modify_for_reopen_catch_up();
  test_startup_recovery_marks_missing_attachments_for_recovered_note_refs();
    test_search_api_returns_matching_hits();
    test_expanded_search_api_returns_body_snippet_and_exact_total_hits();
    test_expanded_search_api_returns_title_only_without_snippet();
    test_expanded_search_api_strips_title_heading_and_collapses_body_whitespace();
    test_expanded_search_api_supports_note_tag_and_path_prefix_filters();
    test_expanded_search_api_supports_exact_offset_limit_pagination();
    test_expanded_search_api_rejects_invalid_page_limits();
    test_expanded_search_api_pagination_tracks_rewrite_and_rebuild();
    test_expanded_search_api_supports_attachment_path_hits_and_missing_flag();
    test_expanded_search_api_supports_kind_all_notes_first_then_attachments();
    test_expanded_search_api_rejects_invalid_filter_and_ranking_combinations_and_clears_stale_output();
    test_expanded_search_api_filters_track_rewrite_and_rebuild();
    test_expanded_search_api_supports_note_ranking_v1_title_boost();
    test_expanded_search_api_supports_note_ranking_v1_single_token_tag_boost();
    test_expanded_search_api_supports_kind_all_ranking_on_note_branch_only();
    test_expanded_search_api_ranking_tracks_rewrite_and_rebuild();
    test_search_api_rewrite_replaces_old_hits();
    test_search_api_accepts_hyphenated_literal_query();
    test_search_api_rejects_whitespace_only_query();
    test_search_api_matches_multiple_literal_tokens_with_extra_whitespace();
  test_search_api_returns_hits_in_rel_path_order();
  test_search_api_returns_one_hit_per_note_even_with_repeated_term();
  test_search_api_matches_title_only_token();
  test_search_api_matches_filename_fallback_title_token();
  test_search_api_reports_title_and_body_match_flags();
  test_search_api_limited_query_rejects_invalid_inputs_and_supports_limit();
  test_tag_query_returns_matching_notes_in_rel_path_order();
  test_backlinks_query_returns_matching_sources();
  test_backlinks_query_accepts_windows_style_relative_path();
  test_tag_and_backlinks_queries_follow_rewrite_recovery_and_rebuild();
  test_tag_query_rejects_invalid_inputs_and_supports_limit();
  test_backlinks_query_rejects_invalid_inputs_and_supports_limit();
  test_background_watcher_indexes_external_create();
  test_background_watcher_updates_external_modify();
  test_background_watcher_applies_external_delete();
  test_close_stops_background_watcher_until_reopen();
  test_close_releases_watcher_handles_so_vault_can_be_renamed();
  test_close_during_delayed_catch_up_does_not_commit_catch_up_results();
  test_internal_write_suppression_does_not_swallow_later_external_modify();
  test_initial_catch_up_and_watcher_poll_do_not_double_apply_external_create();
    test_background_watcher_error_sets_index_unavailable();
    test_background_watcher_recovers_index_state_after_later_success();
    test_background_watcher_recovers_to_ready_without_external_work_and_clears_live_fault();
    test_repeated_watcher_poll_faults_do_not_duplicate_fault_history_before_recovery();
    test_repeated_watcher_poll_faults_back_off_before_auto_recovery();
    test_close_interrupts_watcher_fault_backoff_promptly();
    test_initial_catch_up_failure_degrades_and_then_recovers_index_state();
    test_rebuild_index_reconciles_disk_truth_after_db_drift();
    test_rebuild_reconciles_attachment_missing_state();
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
    test_reopen_catch_up_repairs_partial_state_left_by_interrupted_background_rebuild();
    test_reopen_catch_up_repairs_partial_state_left_by_interrupted_watcher_apply();
    test_background_rebuild_wait_and_join_report_not_found_when_no_task_exists();
    test_get_rebuild_status_reports_idle_then_running_then_success();
    test_get_rebuild_status_reports_current_started_at_while_running();
    test_get_rebuild_status_reports_monotonic_task_generations();
    test_get_rebuild_status_preserves_last_completed_result_while_next_task_runs();
    test_get_rebuild_status_reports_background_failure_result();
    test_export_diagnostics_writes_json_snapshot();
    test_export_diagnostics_reports_recent_events_in_runtime_order();
    test_export_diagnostics_reports_last_continuity_fallback_reason();
    test_export_diagnostics_reports_recovery_outcome_after_corrupt_tail_cleanup();
    test_export_diagnostics_reports_temp_only_recovery_cleanup();
    test_export_diagnostics_reflects_catching_up_without_fault_and_without_stale_count();
    test_export_diagnostics_reflects_rebuilding_without_fault();
    test_export_diagnostics_reflects_watcher_runtime_fault_state();
    test_export_diagnostics_reflects_initial_catch_up_fault_state();
    test_rebuild_failure_sets_unavailable_and_exports_fault();
    test_rebuild_recovers_ready_state_and_clears_fault_fields();
    test_export_diagnostics_reports_last_rebuild_result_and_timestamp();
    test_export_diagnostics_reports_last_attachment_recount_after_watcher_full_rescan();
    test_export_diagnostics_reports_last_rebuild_duration_after_delayed_failure();
    test_export_diagnostics_preserves_last_rebuild_result_code_while_next_task_runs();
    test_export_diagnostics_retains_fault_history_after_recovery();
    test_phase1_alpha_smoke_flow();
    return 0;
  } catch (const std::exception& ex) {
    std::cerr << "kernel_api_tests failed: " << ex.what() << "\n";
    return 1;
  }
}
