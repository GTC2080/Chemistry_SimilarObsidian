// Reason: This file locks the first observable C ABI behaviors before internal modules grow.

#include "kernel/c_api.h"

#include "api/kernel_api_test_suites.h"
#include "api/kernel_api_test_support.h"
#include "core/kernel_internal.h"
#include "index/refresh.h"
#include "recovery/journal.h"
#include "support/test_support.h"
#include "third_party/sqlite/sqlite3.h"

#include <filesystem>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <string>
#include <thread>

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
    test_catching_up_hides_stale_indexed_count_until_reconciled();
    test_open_creates_state_dir_and_storage_db();
    test_write_and_read_roundtrip();
    test_write_appends_save_begin_and_commit_to_journal();
    test_get_state_reflects_persisted_notes_and_recovery_queue();
    run_runtime_recovery_tests();
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
    run_attachment_lifecycle_tests();
    run_search_public_surface_tests();
    test_background_watcher_indexes_external_create();
    test_background_watcher_updates_external_modify();
    test_background_watcher_applies_external_delete();
    test_close_stops_background_watcher_until_reopen();
    test_close_releases_watcher_handles_so_vault_can_be_renamed();
    test_close_during_delayed_catch_up_does_not_commit_catch_up_results();
    test_internal_write_suppression_does_not_swallow_later_external_modify();
    test_initial_catch_up_and_watcher_poll_do_not_double_apply_external_create();
    run_rebuild_runtime_tests();
    // Attachment rebuild missing-state coverage now lives with the attachment lifecycle cluster.
    run_attachment_diagnostics_tests();
    run_runtime_diagnostics_tests();
    test_phase1_alpha_smoke_flow();
    return 0;
  } catch (const std::exception& ex) {
    std::cerr << "kernel_api_tests failed: " << ex.what() << "\n";
    return 1;
  }
}

