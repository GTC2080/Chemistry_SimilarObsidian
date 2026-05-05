// Reason: This file isolates core write contract tests so state and parser coverage can stay smaller.

#include "kernel/c_api.h"

#include "api/kernel_api_core_base_suites.h"
#include "api/kernel_api_test_support.h"
#include "support/test_support.h"
#include "third_party/sqlite/sqlite3.h"

#include <filesystem>
#include <fstream>
#include <string>

namespace {

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

void test_changed_markdown_note_content_filter_and_read_are_kernel_owned() {
  const auto vault = make_temp_vault();
  kernel_handle* handle = nullptr;
  expect_ok(kernel_open_vault(vault.string().c_str(), &handle));

  kernel_note_metadata metadata{};
  kernel_write_disposition disposition{};
  const std::string content = "# Note\nindexed content";
  expect_ok(kernel_write_note(
      handle,
      "Folder/Note.md",
      content.data(),
      content.size(),
      nullptr,
      &metadata,
      &disposition));

  kernel_owned_buffer buffer{};
  expect_ok(kernel_read_first_changed_markdown_note_content(
      handle,
      " Folder\\Note.md \nFolder/Note.txt\nFolder/Note.md",
      &buffer));
  require_true(
      std::string(buffer.data, buffer.size) == content,
      "changed markdown content read should normalize, filter, dedupe, and read the first note");
  kernel_free_buffer(&buffer);

  expect_ok(kernel_read_first_changed_markdown_note_content(handle, "Folder/Note.txt\n", &buffer));
  require_true(buffer.size == 0, "non-markdown changed path should return empty content");
  kernel_free_buffer(&buffer);

  require_true(
      kernel_read_first_changed_markdown_note_content(handle, "missing.md", &buffer).code ==
          KERNEL_ERROR_NOT_FOUND,
      "missing markdown candidate should surface the kernel read error");
  kernel_free_buffer(&buffer);

  require_true(
      kernel_read_first_changed_markdown_note_content(nullptr, "Folder/Note.md", &buffer).code ==
          KERNEL_ERROR_INVALID_ARGUMENT,
      "changed markdown content read should require a handle");
  require_true(
      kernel_read_first_changed_markdown_note_content(handle, "Folder/Note.md", nullptr).code ==
          KERNEL_ERROR_INVALID_ARGUMENT,
      "changed markdown content read should require an output buffer");

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

}  // namespace

void run_kernel_api_core_write_contract_tests() {
  test_write_and_read_roundtrip();
  test_write_appends_save_begin_and_commit_to_journal();
  test_same_content_write_is_noop();
  test_external_edit_causes_conflict();
  test_empty_content_note_is_allowed();
  test_changed_markdown_note_content_filter_and_read_are_kernel_owned();
  test_write_requires_output_pointers();
}
