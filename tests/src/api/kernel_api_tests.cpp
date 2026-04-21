// Reason: This file locks the first observable C ABI behaviors before internal modules grow.

#include "kernel/c_api.h"
#include "api/kernel_api_test_support.h"
#include "core/kernel_internal.h"
#include "core/kernel_shared.h"
#include "index/refresh.h"
#include "recovery/journal.h"
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
void run_search_public_surface_tests();
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
    run_search_public_surface_tests();
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

