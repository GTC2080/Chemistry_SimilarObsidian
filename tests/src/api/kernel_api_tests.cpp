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

void run_attachment_diagnostics_tests();
void run_attachment_public_surface_tests();
void run_attachment_lifecycle_tests();
void test_open_vault_catches_up_external_modify_while_closed();
void test_open_vault_consumes_dangling_sidecar_recovery_record();
void test_open_vault_recovers_unfinished_save_and_cleans_journal();
void test_open_vault_ignores_torn_tail_after_valid_recovery_prefix();
void test_open_vault_ignores_truncated_tail_after_valid_recovery_prefix();
void test_open_vault_ignores_crc_mismatch_tail_after_valid_recovery_prefix();
void test_open_vault_discards_temp_only_unfinished_save();
void test_get_state_ignores_sqlite_diagnostic_recovery_rows();
void test_startup_recovery_prefers_sidecar_truth_over_conflicting_journal_state_rows();
void test_startup_recovery_before_target_replace_keeps_old_disk_truth();
void test_startup_recovery_after_temp_cleanup_recovers_replaced_target_truth();
void test_reopen_catch_up_repairs_stale_derived_state_left_by_interrupted_rebuild();
void test_startup_recovery_replaces_stale_parser_derived_rows();
void test_close_during_watcher_fault_backoff_leaves_delete_for_reopen_catch_up();
void test_close_during_watcher_fault_backoff_leaves_modify_for_reopen_catch_up();
void test_close_during_watcher_fault_backoff_leaves_create_for_reopen_catch_up();
void test_reopen_catch_up_repairs_partial_state_left_by_interrupted_background_rebuild();
void test_reopen_catch_up_repairs_partial_state_left_by_interrupted_watcher_apply();

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
  run_attachment_public_surface_tests();
  test_startup_recovery_replaces_stale_parser_derived_rows();
  run_attachment_lifecycle_tests();
  test_close_during_watcher_fault_backoff_leaves_delete_for_reopen_catch_up();
  test_close_during_watcher_fault_backoff_leaves_modify_for_reopen_catch_up();
  test_close_during_watcher_fault_backoff_leaves_create_for_reopen_catch_up();
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
  // Attachment rebuild missing-state coverage now lives with the attachment lifecycle cluster.
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
    run_attachment_diagnostics_tests();
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
